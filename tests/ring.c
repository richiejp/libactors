#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "actors.h"

enum msg_types {
	MSG_RING = 1,
};

enum addrs {
	ADDR_MIN = 1,
	ADDR_MAX = 10,
};

enum msg_counts {
	MAX_COUNT = 100,
};

struct ring {
	unsigned int count;
};

static void ring_hear(struct actor *, struct msg *);

static struct actor *alloc_ring(addr_t addr)
{
	struct actor *next;

	next = actor_alloc_extra(sizeof(struct ring));
	next->addr = addr;
	next->hear = ring_hear;

	return next;
}

static void ring_hear(struct actor *self, struct msg *msg)
{
	struct ring *my = self->priv;
	addr_t next;

	assert(self->addr >= ADDR_MIN && self->addr <= ADDR_MAX);
	assert(my->count < MAX_COUNT);
	assert(msg->type == MSG_RING);

	if (self->addr == ADDR_MAX)
		next = ADDR_MIN;
	else
		next = self->addr + 1;

	if (!my->count && next > ADDR_MIN)
		actor_start(alloc_ring(self->addr + 1));

	my->count++;

	if (my->count < MAX_COUNT || next != ADDR_MIN)
		actor_say(self, next, msg);

	/* printf("R%lu: %u\n", self->addr, my->count); */

	if (my->count == MAX_COUNT)
		actor_exit(self);
}

int main(void)
{
	struct msg *msg;
	struct actor *first;

	actors_init();

	first = alloc_ring(ADDR_MIN);
	msg = msg_alloc();
	msg->type = MSG_RING;
	msg_box_push(&first->inbox, msg);
	actor_start(first);

	actors_wait();

	return 0;
}
