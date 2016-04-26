#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "include/scsh.h"
#include "include/input.h"
#include "include/bg_task.h"
#include "include/history.h"

#define CURSOL

/*
<line> ::= {<cmd> [';' | '||' | '&&' | '|' | '&']}* <cmd>
<cmd>  ::= <fact> {['<'|'>'|'2>']? <fact>}*
<fact> ::= STR | ''' STR ''' | '"' <fact> '"' | '`' <line> '`' | '&' NUM
*/

char *ptr;
char pwd[BUF_SIZE];

__attribute__((constructor))
init(){
	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	signal (SIGTSTP, sig2child);
	signal (SIGINT,  sig2child);
	signal (SIGCHLD, wait_child);

	if(dup2(STDIN_FILENO, BACKUP_FILENO)<0){
		perror("dup2");
		exit(-1);
	}

	bg_task_init();
	history_init(".scsh_history");
}

__attribute__((destructor))
fini(){
	bg_task_fini();
	history_fini();
	dprintf(STDOUT_FILENO, "exit\n");
}

int main(int argc, char *argv[]){
	if(argc<2){
		char user[BUF_SIZE], host[BUF_SIZE], p;

		getlogin_r(user, sizeof(user));
		gethostname(host, sizeof(host));
		getcwd(pwd, sizeof(pwd));
		p = geteuid() ? '$' : '#';

		while(true){
			char buf[BUF_SIZE]={0}, prompt[BUF_SIZE], term;
			ptr = buf;

			snprintf(prompt, sizeof(prompt), "%s@%s:%s%c ", user, host, pwd, p);
#ifdef CURSOL
			do{
				dprintf(STDOUT_FILENO, "\r\x1b[K%s%s", prompt, buf);
				switch(term = get_line(buf, sizeof(buf), false)){
					case UP:
						history_backward(buf, sizeof(buf));
						break;
					case DOWN:
						history_forward(buf, sizeof(buf));
						break;
					case ENTER:
						history_add(buf, false);
						break;
					case END:
						exit(0);
						break;
				}
			}while(term!=ENTER);
#else
			dprintf(STDOUT_FILENO, "%s", prompt);
			fgets(buf, sizeof(buf),stdin);
#endif
			line(NULL);
		}
	}
	else{
		ptr = argv[1];
		line(NULL);
	}
}

void line(char *buf){
	int status;
	bool cont;
	pid_t cpid, cpgrp;
	struct bg_task *task;
	char *b_ptr = ptr;

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
				waitpid(cpid, &status, WUNTRACED);
				break;
			case AND:
				if(get_token(NULL)!=STR)
					error(__func__);
				else
					unget_token();

				waitpid(cpid, &status, WUNTRACED);
				if(!WIFEXITED(status) || WEXITSTATUS(status))
					cont = false;
				break;
			case OR:
				if(get_token(NULL)!=STR)
					error(__func__);
				else
					unget_token();

				waitpid(cpid, &status, WUNTRACED);
				if(WIFEXITED(status) && !WEXITSTATUS(status))
					cont = false;
				break;
			case PIPE:
				if(get_token(NULL)!=STR && !buf)
					error(__func__);
				else
					unget_token();

				break;
			case BG:
				task = bg_task_add(cpid, cpgrp, cmd, true);
				dprintf(STDERR_FILENO, "[%d] %d\n", task->job_id, cpid);
				break;
			default:
				waitpid(cpid, &status, WUNTRACED);
				cont = false;
		}
		free(cmd);
	}

	if(buf){
		int len;
		memset(buf, 0, BUF_SIZE);
		read(STDIN_FILENO, buf, BUF_SIZE);

		len = strlen(buf);
		if(buf[len-1]=='\n')
			buf[len-1]='\0';
		//dprintf(STDERR_FILENO, "arg : %s\n", buf);
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
				ptr = buf;
				line(p);
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

bool built_in(const char *cmd){
	struct bg_task *task;
	char buf[BUF_SIZE]={0}, token;
	int job_id = -1;

	token = get_token(NULL);
	unget_token();
	if(token==STR)
		factor(buf, NULL);

	if(!(strcmp(cmd, "fg")&&strcmp(cmd, "bg")&&strcmp(cmd, "stop"))){
		job_id = atoi(buf);

		if(job_id>0)
			task = bg_task_entry_byid(job_id);
		else
			task = bg_task_entry_latest();
	}

	if(!strcmp(cmd, "jobs")){
		struct bg_task *task;
		bg_task_for_each(task)
			dprintf(STDERR_FILENO, "[%d] %s\t\t%s\n", task->job_id, task->running ? "Running":"Stopped", task->cmd);
		return true;
	}
	else if(!strcmp(cmd, "fg")){
		if(task){
			int status;

			dprintf(STDERR_FILENO, "%s\n", task->cmd);
			tcsetpgrp(BACKUP_FILENO, task->pgrp);
			killpg(task->pgrp, SIGCONT);
			waitpid(-task->pgrp, &status, WUNTRACED);

			if(!WIFSTOPPED(status))
				bg_task_remove(task);
			else
				task->running = false;
		}
		else
			dprintf(STDERR_FILENO, "not such job\n");
		return true;
	}
	else if(!strcmp(cmd, "bg")){
		if(task){
			dprintf(STDERR_FILENO, "[%d] %s\n", task->job_id, task->cmd);
			killpg(task->pgrp, SIGCONT);
			task->running = true;
		}
		else
			dprintf(STDERR_FILENO, "not such job\n");
		return true;
	}
	else if(!strcmp(cmd, "stop")){
		if(task){
			dprintf(STDERR_FILENO, "[%d] Stopped\t\t%s\n", task->job_id, task->cmd);
			killpg(task->pgrp, SIGTSTP);
			task->running = false;
		}
		else
			dprintf(STDERR_FILENO, "not such job\n");
		return true;
	}
	else if(!strcmp(cmd, "cd")){
		int chdir_ret;
		if(!strlen(buf) || !strcmp(buf, "~"))
			chdir_ret = chdir(getenv("HOME"));
		else if(!strcmp(buf, "-")){
			if(getenv("OLDPWD") && !(chdir_ret = chdir(getenv("OLDPWD"))))
				dprintf(STDOUT_FILENO, "%s\n", getenv("OLDPWD"));
		}
		else
			chdir_ret = chdir(buf);

		if(!chdir_ret){
			setenv("OLDPWD", pwd, true);
			getcwd(pwd, sizeof(pwd));
			setenv("PWD", pwd, true);
		}
		return true;
	}
	else if(!strcmp(cmd, "history")){
		struct hist_cmd* hist;
		int i = 0;

		hist_cmd_for_each(hist)
			dprintf(STDOUT_FILENO, "%d\t%s\n", ++i, hist->cmd);

		return true;
	}
	else if(!strcmp(cmd, "exit")){
		if(!bg_task_isempty())
			dprintf(STDERR_FILENO, "There are stopped jobs.\n");
		else
			exit(0);
		return true;
	}
	else if(!strcmp(cmd, "help")){
		char *msg = 	"scsh - Simple Shell - \n"
				"\n"
				"Command Syntax: \n"
				"\t<line>\t\t:= {<command> [ ';' | '||' | '&&' | '|' | '&' ]}* <command>\n"
				"\t<command>\t:= <program> {[ <arg> | <redirect> ]}*\n"
				"\t<program>\t:= <fact>\n"
				"\t<arg>\t\t:= <fact>\n"
				"\t<redirect>\t:= [ '<' | '>' | '2>' ] [ <file> | '&'<fd> ]\"\n"
				"\t<file>\t\t:= <fact>\n"
				"\t<fd>\t\t:= <num>\n"
				"\t<fact>\t\t:= [ <str> | '\''<str>'\'' | '\"'<fact>'\"' | '`'<line>'`' ]\n"
				"\n"
				"Built-in Command : \n"
				"\tjobs\t\tShow the status of jobs\n"
				"\tfg [jobid]\tSwitch the job running in the background to the foreground\n"
				"\tbg [jobid]\tRun the job in the background\n"
				"\tstop [jobid]\tStop the job that is running in the background\n"
				"\tcd\t\tChange the working directory\n"
				"\thistory\t\tShow command history\n"
				"\thelp\t\tShow this help\n"
				"\texit\t\tExit shell\n";

		dprintf(STDOUT_FILENO, "%s", msg);
		return true;
	}
	
	return false;
}

void sig2child(int signo){}

void wait_child(int signo){
	int status;
	pid_t pid;

	while((pid = waitpid(-1, &status, WNOHANG))>0){
		struct bg_task *task;

		//dprintf(STDERR_FILENO, "pid:%d status:%d\n", pid, status);
		if(task=bg_task_entry_bypid(pid)){
			char *stat = NULL;

			if(WIFEXITED(status))
				stat = "Done";
			else if(WIFSIGNALED(status))
				switch(WTERMSIG(status)){
					case SIGHUP:	stat = "Clean tidyup";	break;
					case SIGINT:	stat = "Interrupt";	break;
					case SIGQUIT:	stat = "Quit";		break;
					case SIGABRT:	stat = "Abort";		break;
					case SIGKILL:	stat = "Die Now";	break;
					case SIGALRM:	stat = "Alarm Clock";	break;
					case SIGTERM:	stat = "Terminated";	break;
					default:	stat = "Signal";
				}

			if(stat){
				dprintf(STDERR_FILENO, "\n[%d] %s\t\t%s\n", task->job_id, stat, task->cmd);
				bg_task_remove(task);
			}
		}
	}
}
