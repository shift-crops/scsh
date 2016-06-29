#ifndef _SCSH_H
#define _SCSH_H

#include "common.h"

void shell(void);
void batch(char *);
bool built_in(const char *);

#endif
