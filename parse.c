#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "include/parse.h"
#include "include/bg_task.h"

/*
<line> ::= {<cmd> [';' | '||' | '&&' | '|' | '&']}* <cmd>
<cmd>  ::= <fact> {['<'|'>'|'2>']? <fact>}*
<fact> ::= STR | ''' STR ''' | '"' <fact> '"' | '`' <line> '`' | '&' NUM
*/

char *ptr;

void line(char *s_buf, char *d_buf){
	int status;
	bool cont;
	pid_t cpid, cpgrp;
	struct bg_task *task;
	char *b_ptr;

	b_ptr = ptr = s_buf;

	for(cpgrp=0, cont = true; cont;){
		char *cmd;
		cmd = strdup(ptr);
		cmd = strtok(cmd, ";&|");

		cpid = command();
		if(!cpid)	continue;
		else if(cpid<0) break;

		if(!cpgrp){
			cpgrp = cpid;
			tcsetpgrp(BACKUP_FILENO, cpgrp);
		}
		setpgid(cpid, cpgrp);

		switch(get_token(NULL)){
			case SEMICOL:
				wait_child(cpid, &status);
				break;
			case AND:
				if(get_token(NULL)!=STR)
					error(__func__);
				else
					unget_token();

				wait_child(cpid, &status);
				if(!WIFEXITED(status) || WEXITSTATUS(status))
					cont = false;
				break;
			case OR:
				if(get_token(NULL)!=STR)
					error(__func__);
				else
					unget_token();

				wait_child(cpid, &status);
				if(WIFEXITED(status) && !WEXITSTATUS(status))
					cont = false;
				break;
			case PIPE:
				if(get_token(NULL)!=STR && !d_buf)
					error(__func__);
				else
					unget_token();

				break;
			case BG:
				task = bg_task_add(cpid, cpgrp, cmd, true);
				dprintf(STDERR_FILENO, "[%d] %d\n", task->job_id, cpid);
				break;
			default:
				wait_child(cpid, &status);
				cont = false;
		}
		free(cmd);
	}

	if(d_buf){
		int len;
		memset(d_buf, 0, BUF_SIZE);
		read(STDIN_FILENO, d_buf, BUF_SIZE-1);

		len = strlen(d_buf);
		if(d_buf[len-1]=='\n')
			d_buf[len-1]='\0';
		//dprintf(STDERR_FILENO, "arg : %s\n", d_buf);
	}

/*
	if(dup2(BACKUP_FILENO, STDIN_FILENO)<0){
		perror("dup2");
		exit(-1);
	}
*/
	dup2(BACKUP_FILENO, STDIN_FILENO);
	tcsetpgrp(BACKUP_FILENO, getpgrp());

	if(WIFSTOPPED(status)){
		struct bg_task *task;
		task = bg_task_add(cpid, cpgrp, b_ptr, false);
		dprintf(STDERR_FILENO, "[%d] Stopped\t\t%s\n", task->job_id, task->cmd);
	}
}

//return value <0:error, 0:buil_in, >0:normal
pid_t command(void){
	int  i;
	int  argc, fd, pfd[2];
	bool cont, next_pipe;
	char buf[BUF_SIZE], token;
	char *b_ptr,**argv, **b_argv, *p;
	pid_t pid;

	b_ptr = ptr;

	token = get_token(NULL);
	if(token==EOL)
		return -1;
	else if(token!=STR && token!=SQUART && token!=DQUART){
		error(__func__);
		return -1;
	}

	unget_token();
	factor(buf, NULL);
	if(built_in(buf))
		return 0;

	for(argc=1, cont=true; cont;){
		switch(token = get_token(NULL)){
			case STR:
			case SQUART:
			case DQUART:
			case BQUART:
				unget_token();
				argc++;
			case STDIN:
			case STDOUT:
			case APPEND:
			case STDERR:
				factor(NULL, NULL);
				break;
			default:
				unget_token();
				cont = false;
		}
	}
	if(next_pipe = (token==PIPE))
		pipe(pfd);

	/*  parent  */
	if((pid = fork())>0){
		if(next_pipe){
			dup2(pfd[0], STDIN_FILENO);
			close(pfd[0]);
			close(pfd[1]);
		}
		return pid;
	}
	/*  error  */
	else if(pid<0){
		perror("fork");
		exit(-1);
	}

	/*  child  */
	if(next_pipe){
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		close(pfd[1]);
	}
	close(BACKUP_FILENO);

	argv = (char**)malloc(sizeof(char*)*(argc+1));
	ptr  = b_ptr;
	for(i=0, cont=true; cont;){
		switch(get_token(NULL)){
			case STDIN:
				factor(buf, NULL);
				close(STDIN_FILENO);
				if(open(buf, O_RDONLY)<0){
					perror(buf);
					_exit(-1);
				}
				break;
			case STDOUT:
				close(STDOUT_FILENO);
				if(factor(buf, &fd)==STR){
					unlink(buf);
					if(open(buf, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)<0){
						perror(buf);
						_exit(-1);
					}
				}
				else
					dup(fd);
				break;
			case APPEND:
				close(STDOUT_FILENO);
				if(factor(buf, &fd)==STR){
					if(open(buf, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)<0){
						perror(buf);
						_exit(-1);
					}
				}
				else
					dup(fd);
				break;
			case STDERR:
				close(STDERR_FILENO);
				if(factor(buf, &fd)==STR){
					unlink(buf);
					if(open(buf, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)<0){
						perror(buf);
						_exit(-1);
					}
				}
				else
					dup(fd);
				break;
			case STR:
			case SQUART:
			case DQUART:
				unget_token();
				factor(buf, NULL);
				argv[i++]=strdup(buf);
				break;
			case BQUART:
				unget_token();
				factor(buf, NULL);

				for(p=buf; *p; p++)
					if(*p=='\n') argc++;

				/*
				b_argv = argv;
				argv = (char**)malloc(sizeof(char*)*(argc+1));
				memcpy(argv, b_argv, sizeof(char*)*i);
				free(b_argv);
				*/
				argv = (char**)realloc(argv, sizeof(char*)*(argc+1));

				argv[i++] = strdup(strtok(buf, "\n"));
				while(p=strtok(NULL, "\n"))
					argv[i++] = strdup(p);
				break;
			default:
				unget_token();
				cont = false;
		}
	}		
	argv[i]=NULL;

	/*
	for(i=0; argv[i]; i++)
		dprintf(STDERR_FILENO, "argv[%d]: %s\n", i, argv[i]);
	*/

	execvp(argv[0],argv);
	perror(argv[0]);
	_exit(-1);
}

int factor(char *p, int *v){
	char c;
	int i;
	char *b_ptr;
	char buf[BUF_SIZE];

	while(get_token(NULL)==SPACE);
	unget_token();

	switch(get_token(&c)){
		case EOL:
			break;
		case STR:
			unget_token();
			for(i=0; i<BUF_SIZE-1 && get_token(&c)==STR; i++)
				if(p)
					*(p++)=c;
			unget_token();
			break;
		case SQUART:
			for(i=0; i<BUF_SIZE-1 && get_token(&c)!=SQUART; i++)
				if(p)
					*(p++)=c;
			break;
		case DQUART:
			for(i=0; i<BUF_SIZE-1 && get_token(&c)!=DQUART; i++)
				if(p){
					if(c=='`'){
						unget_token();
						factor(buf, NULL);

						strncpy(p, buf, BUF_SIZE-i);
						p += strlen(p);
					}
					else
						*(p++)=c;
				}
			break;
		case BQUART:
			for(i=0; i<sizeof(buf)-2 && get_token(&c)!=BQUART; i++)
				buf[i]=c;
			buf[i]='|';
			buf[i+1]='\0';

			if(p){
				b_ptr = ptr;
				line(buf, p);
				ptr = b_ptr;
			}
			return STR;
		case FD:
			b_ptr = ptr;
			for(i=0; i<BUF_SIZE && get_token(&c)==STR; i++)
				if(c<'0' || c>'9'){
					ptr = b_ptr;
					factor(p, NULL);
					return -1;
				}
			unget_token();
			if(v)
				*v = atoi(b_ptr);
			return NUM;
		default:
			unget_token();
	}

	if(p)
		*p='\0';
	return STR;
}

char get_token(char *c){
	if(c)
		*c = *ptr;
	else
		while(*ptr==' ' || *ptr=='\t')
			ptr++;

	switch(*(ptr++)){
		case EOF:
		case '\0':
		case '\n':	return EOL;
		case ';':	return SEMICOL;
		case '|':
			if(*ptr=='|'){
				ptr++;
				return OR;
			}
			else
				return PIPE;
		case '&':
			if(*ptr=='&'){
				ptr++;
				return AND;
			}
			else if(*ptr>='0' && *ptr<='9')
				return FD;
			else
				return BG;
		case '>':
			if(*ptr=='>'){
				ptr++;
				return APPEND;
			}
			else
				return STDOUT;
		case '<':	return STDIN;
		case '1':
			if(*ptr=='>'){
				ptr++;
				return STDOUT;
			}
			else
				return STR;
		case '2':
			if(*ptr=='>'){
				ptr++;
				return STDERR;
			}
			else
				return STR;
		case '\'':	return SQUART;
		case '"':	return DQUART;
		case '`':	return BQUART;
		case ' ':
		case '\t':	return SPACE;
		default:	return STR;
	}
}

void unget_token(void){
	ptr--;

	if((*(ptr-1)=='|' && *ptr=='|') || (*(ptr-1)=='&' && *ptr=='&') || (*(ptr-1)=='>' && *ptr=='>') || (*(ptr-1)=='1' && *ptr=='>') || (*(ptr-1)=='2' && *ptr=='>'))
		ptr--;
}

void error(const char *sym){
	dprintf(STDERR_FILENO, "Error in '%s'\n", sym);
}
