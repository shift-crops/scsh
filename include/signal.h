#ifndef _SIG_H
#define _SIG_H

#include "common.h"

void sig_init(void);
void sig_child(int);
void wait_child(pid_t, int*);
char* sig_status(int);

#endif
