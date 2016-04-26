#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include "include/input.h"

char get_line(char *buf, size_t size, bool reset){
	struct termios io_conf, b_io_conf;
	int idx;
	char term;

	tcgetattr(STDIN_FILENO, &io_conf);
	memcpy(&b_io_conf, &io_conf, sizeof(struct termios));
	io_conf.c_lflag		&= ~(ECHO | ICANON);
	io_conf.c_cc[VMIN]	 = 1;
	io_conf.c_cc[VTIME]	 = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &io_conf);

	for(idx=reset ? 0 : strlen(buf), term=NONE; !term;){
		int len = strlen(buf), key = 0;
		int i;

		if(read(STDIN_FILENO, &key, 1)<1){
			term = END;
			break;
		}

		if(isprint(key)){
			if(len >= size-1)
				continue;

			for(i = len; i>=idx; i--)
				buf[i+1]=buf[i];
			buf[idx]=key;
			dprintf(STDOUT_FILENO, "\x1b[K%s", &buf[idx++]);
			CUR_LEFT(len-idx+1);
		}
		else
			switch(key){
				case 0x09: // Horizontal Tabulation
					dprintf(STDERR_FILENO, "\nCompletion is not implemented.\n");
					term = CONTINUE;
					break;
				case 0x0a: // Line Feed
					write(STDOUT_FILENO, "\n", 1);
					term = ENTER;
					break;
				case 0x1b: // Escape
					read(STDIN_FILENO, ((char*)&key)+1, 3);

					switch(key){
						case KEY_UP:
							//CUR_UP(1);
							term = UP;
							break;
						case KEY_DOWN:
							//CUR_DOWN(1);
							term = DOWN;
							break;
						case KEY_RIGHT:
							if(idx<len){
								idx++;
								CUR_RIGHT(1);
							}
							break;
						case KEY_LEFT:
							if(idx>0){
								idx--;
								CUR_LEFT(1);
							}
							break;
						case KEY_DEL:
							if(idx<len){
								for(i = idx; buf[i]; i++)
									buf[i]=buf[i+1];
								dprintf(STDOUT_FILENO, "\x1b[K%s", &buf[idx]);
								CUR_LEFT(i-idx-1);
							}
							break;
						default:
							dprintf(STDOUT_FILENO, "Undefined ESC key : 0x%x\n", key);
					}
					break;
				case 0x7f: // Delete
					if(idx>0){
						for(i = --idx; buf[i]; i++)
							buf[i]=buf[i+1];
						dprintf(STDOUT_FILENO, "\b\x1b[K%s", &buf[idx]);
						CUR_LEFT(i-idx-1);
					}
					break;
				default:
					dprintf(STDOUT_FILENO, "Undefined key : 0x%x\n", key);
			}
	}

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &b_io_conf);
	return term;
}
