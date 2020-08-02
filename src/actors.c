#include <errno.h>
#include <string.h>

#include "actors.h"

#include <urcu/rculfhash.h>

/* TODO: forwarding addresses */
/* Entry in the the address book */
struct addr_entry {
	addr_t key;
	struct actor *actor;

	/* Chaining in address book hash table */
	struct cds_lfht_node node;
};

/* Actor address book */
static struct cds_lfht *book;

static int addr_entry_match(struct cds_lfht_node *book_node, const void *key)
{
	struct addr_entry *entry =
		caa_container_of(book_node, struct addr_entry, node);

	return *(const addr_t *)key == entry->key;
}

/* Call inside rcu_read_lock() */
static struct addr_entry *book_lookup(addr_t addr)
{
	struct cds_lfht_iter iter;
	struct cds_lfht_node *node;

	cds_lfht_lookup(book, addr, addr_entry_match, &addr, &iter);
	node = cds_lfht_iter_get_node(&iter);

	if (!node)
		return NULL;

	return caa_container_of(node, struct addr_entry, node);
}

void msg_box_init(struct msg_box *box)
{
	cds_wfcq_init(&box->head, &box->tail);
}

void msg_box_push(struct msg_box *box, struct msg *msg)
{
	cds_wfcq_node_init(&msg->node);
	cds_wfcq_enqueue(&box->head, &box->tail, &msg->node);
}

struct msg *msg_box_pop(struct msg_box *box)
{
	struct cds_wfcq_node *node =
		__cds_wfcq_dequeue_blocking(&box->head, &box->tail);

	if (!node)
		return NULL;

	return caa_container_of(node, struct msg, node);
}

/* Remove a message from the actor's inbox.
 *
 * Only a single thread should calls this. This only ever blocks while a
 * message is being enqueued.
 *
 * If a filter is set then only messages which pass the filter will be
 * popped. Others will be buffered until the filter is changed.
 */
struct msg *actor_inbox_pop(struct actor *self)
{
	struct msg *msg;

	for (;;) {
		msg = msg_box_pop(&self->inbox);

		if (!msg)
			break;

		if (!self->filter || self->filter(self, msg))
			break;

		msg_box_push(&self->buf, msg);
	}

	return msg;
}

void actor_inbox_filter(struct actor *self, msg_filter_fn filter)
{
	self->filter = filter;

	__cds_wfcq_splice_blocking(&self->inbox.head,
				   &self->inbox.tail,
				   &self->buf.head,
				   &self->buf.tail);
}

struct msg *_msg_alloc_extra(size_t extra)
{
	struct msg *msg = malloc(sizeof(struct msg) + extra);

	assert_perror(errno);
	assert(msg);

	return msg;
}

/* Allocate a new message and some extra memory for its content ptr */
struct msg *msg_alloc_extra(size_t extra)
{
	struct msg *msg = _msg_alloc_extra(extra);

	assert(extra);
	msg->ptr = (void *)(msg + 1);

	return msg;
}

/* Allocate a new message struct */
struct msg *msg_alloc(void)
{
	return _msg_alloc_extra(0);
}

void actor_hear_loop(struct actor *self)
{
	struct msg *msg;

	assert(self->hear);

	for (;;) {
		msg = actor_inbox_pop(self);

		if (msg)
			self->hear(self, msg);
		else
			/* TODO: futex to sleep properly? */
			pthread_yield();
	}
}

static void *actor_start_routine(void *void_self)
{
	struct actor *self = void_self;
	struct addr_entry *entry;

	rcu_register_thread();

	/* We don't want to send a message with an invalid from address */
	for (;;) {
		rcu_read_lock();
		entry = book_lookup(self->addr);
		rcu_read_unlock();

		if (entry)
			break;

		pthread_yield();
	}

	if (self->listen)
		self->listen(self);
	else
		actor_hear_loop(self);

	actor_exit(self);
}

struct actor *_actor_alloc_extra(size_t extra)
{
	struct actor *actor = malloc(sizeof(*actor) + extra);

	assert_perror(errno);
	assert(actor);

	memset(actor, 0, sizeof(*actor) + extra);

	msg_box_init(&actor->inbox);
	msg_box_init(&actor->buf);

	return actor;
}

struct actor *actor_alloc_extra(size_t extra)
{
	struct actor *actor = _actor_alloc_extra(extra);

	assert(extra);
	actor->priv = (void *)(actor + 1);

	return actor;
}

struct actor *actor_alloc(void)
{
	return _actor_alloc_extra(0);
}

/* Starts the passed actor's thread and adds it to the book.
 *
 * Takes ownership of the passed actor. Returns the new thread. Note that once
 * an actor is started it may free itself (although it does call
 * synchronize_rcu() before free()).
 */
pthread_t actor_start(struct actor *self)
{
	struct addr_entry *entry = malloc(sizeof(*entry));
	struct cds_lfht_node *ret_node;
	pthread_t thread;
	int ret;

	assert_perror(errno);
	assert(entry);
	assert(self->addr);

	entry->key = self->addr;
	entry->actor = self;
	cds_lfht_node_init(&entry->node);

	/* TODO: Thread cancellation and cleanup */
	/* TODO: Set parent actor */
        ret = pthread_create(&self->thread, NULL, actor_start_routine, self);
	assert_perror(ret);
	thread = self->thread;

	rcu_read_lock();
	ret_node = cds_lfht_add_unique(book, entry->key, addr_entry_match,
				       &entry->key, &entry->node);
	assert(ret_node == &entry->node);
	rcu_read_unlock();

	return thread;
}

void actors_init(void)
{
	rcu_register_thread();

	book = cds_lfht_new(1, 1, 0,
			    CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING,
			    NULL);
}

void actors_wait(void)
{
	struct cds_lfht_iter iter;
	struct cds_lfht_node *node;

	do {
		/* TODO: Futex waiters? */
		pthread_yield();

		rcu_read_lock();
		cds_lfht_first(book, &iter);
		node = cds_lfht_iter_get_node(&iter);
		rcu_read_unlock();
	} while (node);
}

void actor_say(struct actor *self, addr_t to, struct msg *msg)
{
	struct addr_entry *entry;

	assert(to);
	assert(msg->type);

	msg->from = self->addr;

	/* Putting enqueue in read section ensures actor still exists */
	rcu_read_lock();
	entry = book_lookup(to);
	assert(entry);
	msg_box_push(&entry->actor->inbox, msg);
	rcu_read_unlock();
}

void actor_exit(struct actor *self)
{
	int ret;
	struct addr_entry *entry;

	rcu_read_lock();
	entry = book_lookup(self->addr);
	ret = cds_lfht_del(book, &entry->node);
	rcu_read_unlock();

	assert(!ret);

	synchronize_rcu();
	free(entry);
	free(self);

	rcu_unregister_thread();

	pthread_exit(self);
}

