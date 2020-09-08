#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "actors.h"

/* If actor_hear_loop doesn't sleep when no messages are queued then
 * this will probably die in a big ball of fire. If it doesn't wake up
 * when a message finally is sent then it will hang.
 */
enum consts {
	SLEEPERS = 500,
};

static void sleeper_hear(struct actor *self, struct msg *msg)
{
	free(msg);
	actor_exit(self);
}

static void waker_listen(struct actor *self)
{
	struct msg *msg;
	int i;
	struct timespec timeout = { 0, 100000 };

	actor_wait(self, &timeout);

	for (i = 0; i < SLEEPERS; i++) {
		msg = msg_alloc();
		msg->type = 1;

		actor_say(self, i + 1, msg);
	}
}

int main(void)
{
	int i;
	struct actor *a;

	actors_init();

	printf("Creating sleepers\n");
	for (i = 0; i < SLEEPERS; i++) {
		a = actor_alloc();
		a->addr = i + 1;
		a->hear = sleeper_hear;

		actor_start(a);
	}

	a = actor_alloc();
	a->addr = SLEEPERS + 2;
	a->listen = waker_listen;

	printf("Creating waker\n");
	actor_start(a);

	actors_wait();

	return 0;
}
