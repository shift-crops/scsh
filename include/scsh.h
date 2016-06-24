#ifndef _SCSH_H
#define _SCSH_H

#include "common.h"

void shell(char *);
bool built_in(const char *);

void sig_child(int);
void wait_child(pid_t, int*);
char* signal_status(int);

#endif
