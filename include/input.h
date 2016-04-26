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

#define NONE			0b000
#define UP			0b001
#define DOWN			0b010
#define CONTINUE		0b100
#define ENTER			0b110
#define END			0b111

char get_line(char *, size_t, bool);

#endif
