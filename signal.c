#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "include/signal.h"
#include "include/bg_task.h"

void sig_init(){
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, sig_child);
	signal(SIGINT,  sig_child);
	signal(SIGCHLD, sig_child);
}

void sig_child(int signo){
	int status;
	pid_t pid;

	while((pid = waitpid(-1, &status, WNOHANG))>0){
		struct bg_task *task;

		//dprintf(STDERR_FILENO, "pid:%d status:%d\n", pid, status);
		if(task=bg_task_entry_bypid(pid)){
			char *stat_msg = sig_status(status);

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

	stat_msg = sig_status(*p_status);
	if(!WIFEXITED(*p_status) && stat_msg)
		dprintf(STDERR_FILENO, "%s\n", stat_msg);
}

char* sig_status(int status){
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
