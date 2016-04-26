#ifndef _HISTORY_H
#define _HISTORY_H

#include "common.h"

#define hist_cmd_for_each(p)	for(p=hist_head.next; p!=&hist_head; p=p->next)

struct hist_cmd{
	char *cmd;
	bool saved;
	struct hist_cmd *prev, *next;
} hist_head;

void history_init(char *);
void history_fini(void);
void history_restore(char *);
void history_save(char *);
void history_add(char *, bool);
void history_backward(char *, size_t size);
void history_forward(char *, size_t size);

#endif
