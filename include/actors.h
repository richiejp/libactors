// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2020 Richard Palethorpe, SUSE LLC */

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <execinfo.h>
#include <pthread.h>

#include <urcu.h>
#include <urcu/rculist.h>
#include <urcu/wfcqueue.h>

#define BT_BUF_SIZE 100

#define actor_assert(expr, fmt, ...) do {				\
	if (expr)							\
		break;							\
									\
	dprintf(STDERR_FILENO, "%s:%s:%i Assert Fail '" #expr "': ",	\
		__FILE__, __func__, __LINE__);				\
	dprintf(STDERR_FILENO, fmt ":\n", ##__VA_ARGS__);		\
									\
	void *buf[BT_BUF_SIZE];						\
	int i, nptrs = backtrace(buf, BT_BUF_SIZE);			\
	for (i = 0; i < nptrs; i++)					\
		dprintf(STDERR_FILENO, "\t%p\n", buf[i]);		\
	exit(1);							\
} while (0);

typedef uint64_t addr_t;

/* Actor message */
struct msg {
	/* User defined message type */
	uint32_t type;
	/* Actor which sent the message */
	addr_t from;
	/* Message content or pointer to the content */
	union {
		char str_val[8];
		uint64_t val;
		void *ptr;
	};

	/* Linked-list node for message queue */
	struct cds_wfcq_node node;
};

struct msg_box {
	struct cds_wfcq_head head;
	struct cds_wfcq_tail tail;
};


struct actor;
typedef int (*msg_filter_fn)(const struct actor *, const struct msg *);

struct actor {
	addr_t addr;

	struct msg_box inbox;
	struct msg_box buf;
	pthread_t thread;
	int32_t futex;

	/* TODO: Cascading actor failures */
	/* addr_t parent; */
	/* struct addr_list children; */

	/* User provided private actor data */
	void *priv;

	void (*listen)(struct actor *);
	void (*hear)(struct actor *, struct msg *);
	msg_filter_fn filter;
};

void actors_init(void);
void actors_wait(void);

/* TODO: Wait/wake on activity to sleep nicely instead of looping on
 * pthread_yield() */

void msg_box_init(struct msg_box *box)
	__attribute__((nonnull));
void msg_box_push(struct msg_box *box, struct msg *msg)
	__attribute__((nonnull));
struct msg *msg_box_pop(struct msg_box *box)
	__attribute__((warn_unused_result, nonnull));

struct msg *msg_alloc(void)
	__attribute__((warn_unused_result));
struct msg *msg_alloc_extra(size_t extra)
	__attribute__((warn_unused_result));

void actor_inbox_push(struct actor *actor, struct msg *msg)
	__attribute__((nonnull));
struct msg *actor_inbox_pop(struct actor *self)
	__attribute__((warn_unused_result, nonnull));
void actor_inbox_filter(struct actor *self, msg_filter_fn filter)
	__attribute__((nonnull (1)));

struct actor *actor_alloc(void)
	__attribute__((warn_unused_result));
struct actor *actor_alloc_extra(size_t extra)
	__attribute__((warn_unused_result));
void actor_wait(struct actor *self)
	__attribute__((nonnull));
void actor_hear_loop(struct actor *self)
	__attribute__((nonnull, noreturn));
pthread_t actor_start(struct actor *self)
	__attribute__((nonnull));
void actor_say(struct actor *self, addr_t to, struct msg *msg)
	__attribute__((nonnull));
unsigned int actor_exists(addr_t it)
	__attribute__((warn_unused_result));

void actor_exit(struct actor *self)
	__attribute__((nonnull, noreturn));
/* TODO: void actor_join(addr_t who); */

