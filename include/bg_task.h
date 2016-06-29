#ifndef _BG_TASK_H
#define _BG_TASK_H

#include "common.h"

#define bg_task_for_each(head, p)	for(p=head.next; p!=&head; p=p->next)
#define bg_task_isempty(head)		(head.next==&head)

struct bg_task{
	int job_id;
	pid_t pid, pgrp;
	char *cmd;
	bool running;
	struct bg_task *prev, *next;
} bg_head;

void bg_task_init(void);
void bg_task_fini(void);
struct bg_task* bg_task_add(pid_t, pid_t, char *, bool);
int bg_task_remove(struct bg_task*);
struct bg_task* bg_task_entry_byid(int);
struct bg_task* bg_task_entry_bypid(pid_t);
struct bg_task* bg_task_entry_latest(void);

#endif
