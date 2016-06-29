#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include "include/input.h"

struct termios io_conf, b_io_conf;

void input_init(void){
	tcgetattr(STDIN_FILENO, &io_conf);
	memcpy(&b_io_conf, &io_conf, sizeof(struct termios));
	io_conf.c_lflag		&= ~(ECHO | ICANON);
	io_conf.c_cc[VMIN]	 = 1;
	io_conf.c_cc[VTIME]	 = 0;
}

char get_line(char *buf, size_t size, bool reset, int *position){
	int idx;
	char term;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &io_conf);

	if(reset) buf[0]='\0';
	for(idx=strlen(buf), term=NONE; !term;){
		int len = strlen(buf), i;
		char key, c;

		switch(key = keycode(&c)){
			case LETR:
				if(len >= size-1)
					break;

				for(i = len; i>=idx; i--)
					buf[i+1]=buf[i];
				buf[idx]=c;
				dprintf(STDOUT_FILENO, "\x1b[K%s", &buf[idx++]);
				CUR_LEFT(len-idx+1);
				break;
			case RIGHT:
				if(idx<len){
					idx++;
					CUR_RIGHT(1);
				}
				break;
			case LEFT:
				if(idx>0){
					idx--;
					CUR_LEFT(1);
				}
				break;
			case DEL:
				if(idx<len){
					for(i = idx; buf[i]; i++)
						buf[i]=buf[i+1];
					dprintf(STDOUT_FILENO, "\x1b[K%s", &buf[idx]);
					CUR_LEFT(i-idx-1);
				}
				break;
			case BS:
				if(idx>0){
					for(i = --idx; buf[i]; i++)
						buf[i]=buf[i+1];
					dprintf(STDOUT_FILENO, "\b\x1b[K%s", &buf[idx]);
					CUR_LEFT(i-idx-1);
				}
				break;
			case TAB:
			case ENTER:
			case UP:
			case DOWN:
			case END:
				term = key;
				break;
		}
	}

	if(position)
		*position = idx;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &b_io_conf);
	return term;
}

char get_term(void){
	char term;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &io_conf);
	term = keycode(NULL);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &b_io_conf);

	return term;
}

char keycode(char *letter){
	int key = 0;

	if(read(STDIN_FILENO, &key, 1)<1)
		return END;

	if(isprint(key)){
		if(letter) *letter = (char)key;
		return LETR;
	}

	switch(key){
		case 0x09: // Horizontal Tabulation
			return TAB;
		case 0x0a: // Line Feed
			write(STDOUT_FILENO, "\n", 1);
			return ENTER;
		case 0x1b: // Escape
			read(STDIN_FILENO, ((char*)&key)+1, 3);

			switch(key){
				case KEY_UP:
					return UP;
				case KEY_DOWN:
					return DOWN;
				case KEY_RIGHT:
					return RIGHT;
				case KEY_LEFT:
					return LEFT;
				case KEY_DEL:
					return DEL;
				default:
					dprintf(STDOUT_FILENO, "Undefined ESC key : 0x%x\n", key);
			}
			break;
		case 0x7f: // Delete
			return BS;
		default:
			dprintf(STDOUT_FILENO, "Undefined key : 0x%x\n", key);
	}

	return UNDEF;
}
