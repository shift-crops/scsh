#include <stdlib.h>
#include <string.h>
#include "include/bg_task.h"

void bg_task_init(void){
	bg_head.job_id	= 0;
	bg_head.pgrp	= 0;
	bg_head.cmd	= NULL;
	bg_head.prev	= &bg_head;
	bg_head.next	= &bg_head;
}

void bg_task_fini(void){
	struct bg_task *p;

	bg_task_for_each(bg_head, p)
		if(p->prev!=&bg_head){
			free(p->prev->cmd);
			free(p->prev);
		}
}

struct bg_task* bg_task_add(pid_t pid, pid_t pgrp, char *cmd, bool running){
	struct bg_task *p, *task;
	char *lf;

	for(p=&bg_head; p->next!=&bg_head; p=p->next)
		if(p->next->job_id-p->job_id > 1)
			break;

	task = (struct bg_task*)malloc(sizeof(struct bg_task));
	task->job_id	= p->job_id+1;
	task->pid	= pid;
	task->pgrp	= pgrp;
	task->cmd	= strdup(cmd);
	task->running 	= running;
	task->prev	= p;
	task->next	= p->next;

	p->next->prev	= task;
	p->next		= task;

	lf = strchr(task->cmd, '\n');
	if(lf)	*lf='\0';

	return task;
}

int bg_task_remove(struct bg_task* task){
	int job_id = task->job_id;

	task->prev->next = task->next;
	task->next->prev = task->prev;
	free(task->cmd);
	free(task);

	return job_id;
}

struct bg_task* bg_task_entry_byid(int job_id){
	struct bg_task *task;

	bg_task_for_each(bg_head, task)
		if(task->job_id == job_id)
			return task;
	return NULL;
}

struct bg_task* bg_task_entry_bypid(pid_t pid){
	struct bg_task *task;

	bg_task_for_each(bg_head, task)
		if(task->pid == pid)
			return task;
	return NULL;
}

struct bg_task* bg_task_entry_latest(void){
	return bg_task_isempty(bg_head) ? NULL : bg_head.prev;
}
