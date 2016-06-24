#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <getopt.h>
#include "include/scsh.h"
#include "include/input.h"
#include "include/parse.h"
#include "include/bg_task.h"
#include "include/history.h"

#define CURSOL

char cwd[BUF_SIZE];

__attribute__((constructor))
init(){
	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	signal (SIGTSTP, sig_child);
	signal (SIGINT,  sig_child);
	signal (SIGCHLD, sig_child);

	if(dup2(STDIN_FILENO, BACKUP_FILENO)<0){
		perror("dup2");
		exit(-1);
	}

	input_init();
	bg_task_init();
	history_init(".scsh_history");
}

__attribute__((destructor))
fini(){
	bg_task_fini();
	history_fini();
}

int main(int argc, char *argv[]){
	char opt;
	char *msg = "scsh, version %s (Compiled at %s %s)\n", *version="1.01";

	struct option long_options[] = {
	        {"help",     no_argument, NULL, 'h'},
	        {"version",  no_argument, NULL, 'v'},
	        {0, 0, 0, 0}};

	switch(getopt_long(argc, argv, "-c:hv", long_options, NULL)){
		case -1:
			shell(NULL);
			break;
		case 'c':
			line(optarg, NULL);
			break;
		case 'v':
			dprintf(STDOUT_FILENO, msg, version, __DATE__, __TIME__);

			msg =	"Copyright (C) %s ShiftCrops\n"
				"\n"
				"This software is released under the MIT License.\n"
				"http://opensource.org/licenses/mit-license.php\n"
				"\n";
			dprintf(STDOUT_FILENO, msg, "2016");
			break;
		case 'h':
		case '?':
			dprintf(STDOUT_FILENO, msg, version, __DATE__, __TIME__);

			msg =	"Usage:"
				"\t%1$s [option]...\n"
				"\t%1$s script-file...\n"
				"\n"
				"Options:\n"
				"\t-c command\n"
				"\t\texecute command\n"
				"\t-v, --version\n"
				"\t\tshow version information\n"
				"\t-h, --help\n"
				"\t\tshow this help\n"
				"\n";
			dprintf(STDOUT_FILENO, msg, argv[0]);
			break;
		default:
			shell(optarg);
	}
}

void shell(char *fname){
	if(fname){
		FILE *fp;
		char buf[BUF_SIZE]={0};

		if(fp = fopen(fname, "r")){
			while(fgets(buf, sizeof(buf), fp))
				line(buf, NULL);
			fclose(fp);
		}
		else
			perror(fname);
	}
	else{
		char user[BUF_SIZE]={0}, host[BUF_SIZE]={0}, p;
		struct passwd *pw;
		uid_t uid;

		uid = geteuid();
		if(pw = getpwuid(uid))
			strncpy(user, pw->pw_name, sizeof(user));
		//getlogin_r(user, sizeof(user));
		gethostname(host, sizeof(host));
		getcwd(cwd, sizeof(cwd));
		p = uid ? '$' : '#';

		while(true){
			char buf[BUF_SIZE]={0}, prompt[BUF_SIZE], term;

			snprintf(prompt, sizeof(prompt), "%s@%s:%s%c ", user, host, cwd, p);
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
					case TAB:
						dprintf(STDERR_FILENO, "\nCompletion is not implemented.\n");
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
			line(buf, NULL);
		}
		dprintf(STDOUT_FILENO, "exit\n");
	}
}

bool built_in(const char *cmd){
	struct bg_task *task;
	char buf[BUF_SIZE]={0}, token;
	int job_id = -1;

	token = get_token(NULL);
	unget_token();
	if(token==STR || token==SQUART || token==DQUART)
		factor(buf, NULL);

	if(!(strcmp(cmd, "fg")&&strcmp(cmd, "bg")&&strcmp(cmd, "stop"))){
		job_id = atoi(buf);

		if(job_id>0)
			task = bg_task_entry_byid(job_id);
		else
			task = bg_task_entry_latest();
	}

	if(!strcmp(cmd, "jobs")){
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
			setenv("OLDPWD", cwd, true);
			getcwd(cwd, sizeof(cwd));
			setenv("PWD", cwd, true);
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

void sig_child(int signo){
	int status;
	pid_t pid;

	//dprintf(STDERR_FILENO, "%s(%d)\n", __func__, signo);
	while((pid = waitpid(-1, &status, WNOHANG))>0){
		struct bg_task *task;

		//dprintf(STDERR_FILENO, "pid:%d status:%d\n", pid, status);
		if(task=bg_task_entry_bypid(pid)){
			char *stat_msg = signal_status(status);

			if(stat_msg){
				dprintf(STDERR_FILENO, "\n[%d] %s\t\t%s\n", task->job_id, stat_msg, task->cmd);
				bg_task_remove(task);
			}
		}
	}
}

void wait_child(pid_t pid, int *p_status){
	char *stat_msg;

	waitpid(pid, p_status, WUNTRACED);

	stat_msg = signal_status(*p_status);
	if(!WIFEXITED(*p_status) && stat_msg)
		dprintf(STDERR_FILENO, "%s\n", stat_msg);
}

char* signal_status(int status){
	char *stat_msg = NULL;

	if(WIFEXITED(status))
		stat_msg = "Done";
	else if(WIFSIGNALED(status))
		switch(WTERMSIG(status)){
			case SIGHUP:	stat_msg = "Hangup";		break;
			case SIGINT:	stat_msg = "Interrupt";		break;
			case SIGQUIT:	stat_msg = "Quit";		break;
			case SIGABRT:	stat_msg = "Abort";		break;
			case SIGKILL:	stat_msg = "Die Now";		break;
			case SIGALRM:	stat_msg = "Alarm Clock";	break;
			case SIGTERM:	stat_msg = "Terminated";	break;
			default:	stat_msg = "Signal";
		}

	return stat_msg;
}
