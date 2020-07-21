#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#include <urcu.h>
#include <urcu/rculist.h>
#include <urcu/wfcqueue.h>

typedef uint64_t addr_t;

/* Actor message */
struct msg {
	/* User defined message type */
	int type;
	/* Actor which sent the message */
	addr_t from;
	/* Message content or pointer to the content */
	union {
		uint64_t val;
		void *ptr;
	};

	/* Linked-list node for message queue */
	struct cds_wfcq_node node;
};

struct actor;
struct actor {
	addr_t addr;

	struct cds_wfcq_head inbox_head;
	struct cds_wfcq_tail inbox_tail;
	pthread_t thread;

	/* TODO: Cascading actor failures */
	addr_t parent;
	struct cds_list_head children;

	/* User provided private actor data */
	void *priv;

	void (*listen)(struct actor *);
	void (*hear)(struct actor *, struct msg *);
};

void actors_init(void);

struct msg *msg_alloc(void);

struct msg *actor_inbox_pop(struct actor *self);

struct actor *actor_alloc(void);
void actor_hear_loop(struct actor *self);
pthread_t actor_start(struct actor *self);
void actor_say(struct actor *self, addr_t to, struct msg *msg);
__attribute__((noreturn))
void actor_exit(struct actor *self);
/* TODO: void actor_join(addr_t who); */
