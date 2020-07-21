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

/* TODO: What if address doesn't exist? */
	assert(node);

	return caa_container_of(node, struct addr_entry, node);
}

/* Remove a message from the actor's inbox.
 *
 * Only a single thread should calls this. This only ever blocks while a
 * message is being enqueued.
 */
struct msg *actor_inbox_pop(struct actor *self)
{
	struct cds_wfcq_node *node;

	node = __cds_wfcq_dequeue_blocking(&self->inbox_head,
					   &self->inbox_tail);

	if (node)
		return caa_container_of(node, struct msg, node);
	else
		return NULL;
}

/* Allocate a new message struct */
struct msg *msg_alloc(void)
{
	struct msg *msg = malloc(sizeof(*msg));

	assert_perror(errno);
	assert(msg);

	memset(msg, 0, sizeof(*msg));

	return msg;
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

	rcu_register_thread();

	/* Wait for our address to be added to the book */
	synchronize_rcu();

	if (self->listen)
		self->listen(self);
	else
		actor_hear_loop(self);

	actor_exit(self);
}

struct actor *actor_alloc(void)
{
	struct actor *actor = malloc(sizeof(*actor));

	assert_perror(errno);
	assert(actor);

	memset(actor, 0, sizeof(*actor));

	return actor;
}

/* Starts the passed actor's thread and adds it to the book
 *
 * Takes ownership of the passed actor. Returns the new thread.
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

	cds_wfcq_init(&self->inbox_head, &self->inbox_tail);

	/* TODO: Thread cancellation and cleanup */
	/* TODO: Set parent actor */

	/* We want the new actor to wait at synchronize for this read section
	 * to end. Otherwise the new actor could send itself a message before
	 * it has been added to he address book.
	 *
	 * TODO: Perhaps it would be better to wait on a mutex or message?
	 */
	rcu_read_lock();
        ret = pthread_create(&self->thread, NULL, actor_start_routine, self);
	thread = self->thread;

	ret_node = cds_lfht_add_unique(book, entry->key, addr_entry_match,
				       &entry->key, &entry->node);
	rcu_read_unlock();

	assert_perror(ret);
	assert(ret_node == &entry->node);

	return thread;
}

void actors_init(void)
{
	rcu_register_thread();

	book = cds_lfht_new(1, 1, 0,
			    CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING,
			    NULL);
}

void actor_say(struct actor *self, addr_t to, struct msg *msg)
{
	struct addr_entry *entry;

	assert(to);
	assert(msg->type);

	msg->from = self->addr;
	cds_wfcq_node_init(&msg->node);

	/* Putting enqueue in read section ensures actor still exists */
	rcu_read_lock();
	entry = book_lookup(to);
	cds_wfcq_enqueue(&entry->actor->inbox_head,
			 &entry->actor->inbox_tail,
			 &msg->node);
	rcu_read_unlock();
}

__attribute__((noreturn))
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
	free(self->priv);
	free(self);

	rcu_unregister_thread();

	pthread_exit(self);
}
