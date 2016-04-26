#ifndef _SCSH_H
#define _SCSH_H

#include "common.h"

#define BACKUP_FILENO	255

#define SPACE		0b00000

#define STR		0b00010
#define NUM		0b00011

#define SQUART		0b00100
#define DQUART		0b00101
#define BQUART		0b00110

#define STDIN		0b01000
#define STDOUT		0b01001
#define APPEND		0b01010
#define STDERR		0b01011

#define SEMICOL		0b10000
#define AND		0b10001
#define OR		0b10010
#define PIPE		0b10011
#define BG		0b10100
#define FD		0b10101

#define EOL		0b11111

void line(char *);
pid_t command(void);
int factor(char *, int *);
char get_token(char *);
void unget_token(void);
void error(const char *);

bool built_in(const char *);

void sig2child(int);
void wait_child(int);

#endif
