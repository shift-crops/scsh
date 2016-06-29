#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <getopt.h>
#include "include/scsh.h"
#include "include/signal.h"
#include "include/input.h"
#include "include/complete.h"
#include "include/parse.h"
#include "include/bg_task.h"
#include "include/history.h"

#define CURSOL

char cwd[BUF_SIZE];

__attribute__((constructor))
init(){
	if(dup2(STDIN_FILENO, BACKUP_FILENO)<0){
		perror("dup2");
		exit(-1);
	}

	sig_init();
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
	char *msg = "scsh, version %s (Compiled at %s %s)\n", *version="2.00";

	struct option long_options[] = {
	        {"help",     no_argument, NULL, 'h'},
	        {"version",  no_argument, NULL, 'v'},
	        {0, 0, 0, 0}};

	switch(getopt_long(argc, argv, "-c:hv", long_options, NULL)){
		case -1:
			shell();
			break;
		case 'c':
			line(optarg, NULL, 0);
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
			batch(optarg);
	}
}

void shell(void){
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
		int pos;

		snprintf(prompt, sizeof(prompt), "%s@%s:%s%c ", user, host, cwd, p);
#ifdef CURSOL
		do{
			dprintf(STDOUT_FILENO, "\r\x1b[K%s%s", prompt, buf);
			switch(term = get_line(buf, sizeof(buf), false, &pos)){
				case UP:
					history_backward(buf, sizeof(buf));
					break;
				case DOWN:
					history_forward(buf, sizeof(buf));
					break;
				case TAB:
					completion(pos, buf, sizeof(buf));
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
		line(buf, NULL, 0);
	}
	dprintf(STDOUT_FILENO, "exit\n");
}

void batch(char *fname){
	FILE *fp;
	char buf[BUF_SIZE]={0};

	if(fp = fopen(fname, "r")){
		while(fgets(buf, sizeof(buf), fp))
			line(buf, NULL, 0);
		fclose(fp);
	}
	else
		perror(fname);
}

bool built_in(const char *cmd){
	struct bg_task *task;
	char buf[BUF_SIZE]={0}, token;
	int job_id = -1;

	token = get_token(NULL);
	unget_token();
	if(token==STR || token==SQUART || token==DQUART)
		factor(buf, sizeof(buf), NULL);

	if(!(strcmp(cmd, "fg")&&strcmp(cmd, "bg")&&strcmp(cmd, "stop"))){
		job_id = atoi(buf);

		if(job_id>0)
			task = bg_task_entry_byid(job_id);
		else
			task = bg_task_entry_latest();
	}

	if(!strcmp(cmd, "jobs")){
		bg_task_for_each(bg_head, task)
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
		char *hist[BUF_SIZE];
		int i, size_hist;

		size_hist = history_list(hist, sizeof(hist));
		for(i=0; i<size_hist; i++)
			dprintf(STDOUT_FILENO, "%d\t%s\n", i+1, hist[i]);

		return true;
	}
	else if(!strcmp(cmd, "exit")){
		if(bg_task_entry_latest())
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
