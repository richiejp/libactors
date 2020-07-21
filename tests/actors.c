#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "actors.h"

#define MSG_FOO 1
#define MSG_BAZ 2

#define ADDR_A 1
#define ADDR_B 2

void a_listen(struct actor *self)
{
	struct msg *msg = msg_alloc();

	msg->type = MSG_FOO;
	msg->ptr = "bar";

	printf("A: Say 'bar'\n");
	actor_say(self, ADDR_B, msg);
	usleep(100);

	while (!(msg = actor_inbox_pop(self))) {
		pthread_yield();
	}

	assert(msg->type == MSG_BAZ);
	printf("A: Hear '%s'\n", (char *)msg->ptr);

	free(msg);

	printf("A: Exit\n");
}

void b_hear(struct actor *self, struct msg *msg)
{
	assert(msg->type == MSG_FOO);
	printf("B: Hear '%s'\n", (char *)msg->ptr);

	msg->type = MSG_BAZ;
	msg->ptr = "baz";

	printf("B: Say 'baz'\n");
	actor_say(self, msg->from, msg);

	printf("B: Exit\n");
	actor_exit(self);
}

int main(void/* int argc, char **argv */)
{
	int r0, r1;
	struct actor *a, *b;
	pthread_t at, bt;

	printf("M: libactors v%d.%d\n",
	       libactors_VERSION_MAJOR, libactors_VERSION_MINOR);
	actors_init();

	a = actor_alloc();
	a->addr = ADDR_A;
	a->listen = a_listen;

	b = actor_alloc();
	b->addr = ADDR_B;
	b->hear = b_hear;

	printf("M: Start B\n");
	at = actor_start(b);
	printf("M: Start A\n");
	bt = actor_start(a);

	r0 = pthread_join(at, NULL);
	printf("M: Joined A\n");
	r1 = pthread_join(bt, NULL);
	printf("M: Joined B\n");

	assert_perror(r0);
	assert_perror(r1);

	rcu_unregister_thread();

	return 0;
}
