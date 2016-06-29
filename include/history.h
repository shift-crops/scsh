#ifndef _HISTORY_H
#define _HISTORY_H

#include "common.h"

#define hist_cmd_for_each(head, p)	for(p=head.next; p!=&head; p=p->next)

struct hist_cmd{
	char *cmd;
	bool saved;
	struct hist_cmd *prev, *next;
};

void history_init(char *);
void history_fini(void);
void history_restore(char *);
void history_save(char *);
void history_add(char *, bool);
void history_backward(char *, size_t);
void history_forward(char *, size_t);
int history_list(char **, int);

#endif
