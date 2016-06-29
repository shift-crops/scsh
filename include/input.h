#ifndef _INPUT_H
#define _INPUT_H

#include "common.h"

#define KEY_UP			0x00415b1b
#define KEY_DOWN		0x00425b1b
#define KEY_RIGHT		0x00435b1b
#define KEY_LEFT		0x00445b1b
#define KEY_DEL			0x7e335b1b

#define MOVE_CURSOL(s, n)	if(n>0)dprintf(STDOUT_FILENO, s, n)
#define CUR_UP(n)		MOVE_CURSOL("\x1b[%dA", (int)(n))
#define CUR_DOWN(n)		MOVE_CURSOL("\x1b[%dB", (int)(n))
#define CUR_RIGHT(n)		MOVE_CURSOL("\x1b[%dC", (int)(n))
#define CUR_LEFT(n)		MOVE_CURSOL("\x1b[%dD", (int)(n))

#define NONE			0b0000
#define UNDEF			0b0001
#define UP			0b0100
#define DOWN			0b0101
#define RIGHT			0b0110
#define LEFT			0b0111
#define ENTER			0b1000
#define TAB			0b1001
#define BS			0b1010
#define DEL			0b1011
#define LETR			0b1100
#define END			0b1111

char get_line(char *, size_t, bool);
char get_term(void);
char keycode(char *);

#endif
