#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "actors.h"

enum msg_types {
	MSG_NUL,
	MSG_FOO,
	MSG_BAR,
	MSG_BAZ,
};

#define ADDR_A 1
#define ADDR_B 2

void a_hear(struct actor *self, struct msg *msg)
{
	static enum msg_types seen = MSG_NUL;

	switch(msg->type) {
	case MSG_FOO:
		printf("A: Seen foo\n");
		assert(seen == MSG_BAZ);
		seen = MSG_FOO;
		free(msg);
		break;
	case MSG_BAR:
		printf("A: Seen bar\n");
		assert(seen == MSG_FOO);
		free(msg);
		actor_exit(self);
		break;
	case MSG_BAZ:
		printf("A: Seen baz\n");
		assert(seen == MSG_NUL);
		seen = MSG_BAZ;
		free(msg);
		actor_inbox_filter(self, NULL);
		break;
	default:
		assert(0);
	}
}

static int only_baz(const struct actor *self __attribute__((unused)),
		    const struct msg *msg)
{
	return msg->type == MSG_BAZ;
}

int main(void)
{
	int r0;
	struct actor *a;
	pthread_t at;
	struct msg *msgs[] = {
		msg_alloc(),
		msg_alloc(),
		msg_alloc()
	};

	actors_init();

	a = actor_alloc();
	a->addr = ADDR_A;
	a->hear = a_hear;
	printf("M: Alloc A\n");

	msgs[0]->type = MSG_FOO;
	msg_box_push(&a->inbox, msgs[0]);
	msgs[1]->type = MSG_BAR;
	msg_box_push(&a->inbox, msgs[1]);
	msgs[2]->type = MSG_BAZ;
	msg_box_push(&a->inbox, msgs[2]);
	printf("M: Pushed msgs\n");

	actor_inbox_filter(a, only_baz);

	printf("M: Start A\n");
	at = actor_start(a);

	r0 = pthread_join(at, NULL);
	printf("M: Joined A\n");

	assert_perror(r0);

	rcu_unregister_thread();

	return 0;
}
