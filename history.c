#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/history.h"

struct hist_cmd hist_head, *hist_ptr;
char hist_file[BUF_SIZE];

void history_init(char *fname){
	hist_ptr 	= &hist_head;

	hist_head.cmd	= NULL;
	hist_head.prev	= &hist_head;
	hist_head.next	= &hist_head;

	snprintf(hist_file, sizeof(hist_file), "%s/%s", getenv("HOME"), fname);
	history_restore(hist_file);
}

void history_fini(void){
	struct hist_cmd *p;

	history_save(hist_file);

	hist_cmd_for_each(hist_head, p)
		if(p->prev!=&hist_head){
			free(p->prev->cmd);
			p->prev->cmd = NULL;
			free(p->prev);
		}

	hist_ptr = NULL;
}

void history_restore(char *fname){
	FILE *fp;
	char buf[BUF_SIZE];

	if((fp = fopen(fname, "r"))==NULL)
		return;

	while(fgets(buf, sizeof(buf), fp))
		history_add(strtok(buf, "\n"), true);

	fclose(fp);
}

void history_save(char *fname){
	FILE *fp;
	struct hist_cmd *hist;

	if((fp = fopen(fname, "a"))==NULL)
		return;

	hist_cmd_for_each(hist_head, hist)
		if(!hist->saved)
			fprintf(fp, "%s\n", hist->cmd);

	fclose(fp);
}

void history_add(char *buf, bool saved){
	struct hist_cmd *hist;

	if(!buf || !strlen(buf))
		return;

	hist_ptr = &hist_head;

	hist = (struct hist_cmd*)malloc(sizeof(struct hist_cmd));
	hist->cmd		= strdup(buf);
	hist->saved		= saved;
	hist->prev		= hist_head.prev;
	hist->next		= &hist_head;

	hist_head.prev->next	= hist;
	hist_head.prev		= hist;
}

void history_backward(char *buf, size_t size){
	if(!buf || hist_ptr->prev==&hist_head)
		return;

	hist_ptr = hist_ptr->prev;
	if(hist_ptr->cmd)
		strncpy(buf, hist_ptr->cmd, size);
}

void history_forward(char *buf, size_t size){
	if(!buf || hist_ptr==&hist_head)
		return;

	hist_ptr = hist_ptr->next;
	if(hist_ptr==&hist_head)
		buf[0]='\0';
	else if(hist_ptr->cmd)
		strncpy(buf, hist_ptr->cmd, size);
}

int history_list(char *cmd[], int max){
	struct hist_cmd* hist;
	int i = 0;

	hist_cmd_for_each(hist_head, hist)
		if(i<max)
			cmd[i++]=hist->cmd;
		else
			break;

	return i;
}
