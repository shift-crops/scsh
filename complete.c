#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include "include/complete.h"

#define MIN_SPACE 16

void completion(int pos, char *line, size_t line_size){
	char *remain, *p;
	char b_c, keyword[BUF_SIZE];

	if(!strlen(line))
		return;

	remain = strdup(line+pos);

	b_c = line[pos];
	line[pos]='\0';

	if(p = strrchr(line, ' '))	p++;
	else				p = line;

	strncpy(keyword, p, sizeof(keyword));
	line[pos] = b_c;

	//dprintf(STDERR_FILENO, "\nCompletion(%d) : %s\n", pos, p);

	if(search(keyword, sizeof(keyword))){
		strncpy(p, keyword, line_size-(p-line));
		strncat(line, remain, line_size-strlen(line));
	}

	free(remain);
}

bool search(char *line, size_t line_size){
	char path[PATH_MAX], buf[BUF_SIZE]={0};
	char *env_path, *piece, *fname, *p;
	char **files;
	int count,i;

	//dprintf(STDERR_FILENO, "\nCompletion : %s\n", line);

	if(piece = strrchr(line, '/'))	piece++;
	else				piece = line;

	if(piece-line)
		strncpy(buf, line, piece-line);
	else
		buf[0]='.';
	realpath(buf, path);

	count = candidate(&files, 0, path, piece);
	if(!count && !strchr(line, '/')){
		env_path = strdup(getenv("PATH"));
		for(p = strtok(env_path, ":"); p; p=strtok(NULL, ":"))
			count = candidate(&files, count, p, piece);
		free(env_path);
	}

	if(!count)
		return false;


	if(p = strrchr(line, '/'))	p++;
	else				p = line;

	if(count==1){
		strncpy(p, files[0], line_size-(p-line));
		free(files[0]);
	}
	else{
		struct winsize ws;
		int line_n, max_len;
		char format[8];

		strncpy(buf, files[0], sizeof(buf));
		max_len = 0;
		for(i=0; i<count; i++){
			int match_n;

			match_n = strmatch(buf, files[i]);
			buf[match_n]='\0';

			if(strlen(files[i])>max_len)
				max_len = strlen(files[i]);
		}

		/*
		printf("p: %s\tbuf:%s\n", p, buf);
		for(i=0; i<count; i++)
			printf("%02d: %s\n", i, files[i]);
		*/

		if(strlen(buf) && strcmp(p, buf))
			strncpy(p, buf, line_size-(p-line));
		else{
			int length;

			length = max_len + (8-max_len%8);
			if(length < MIN_SPACE)
				length = MIN_SPACE;

			snprintf(format, sizeof(format), "\n%%-%ds", length);
			line_n = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)!=-1 && ws.ws_col > 0) ? ws.ws_col/length : 1;

			for(i=0; i<count; i++){
				dprintf(STDERR_FILENO, format+(i%line_n ? 1 : 0), files[i]);
				free(files[i]);
			}
			dprintf(STDERR_FILENO, "\n");
		}
	}

	free(files);
	return true;
}

int candidate(char **files[], int nfile, char *path, char *piece){
	DIR *dir;
	struct dirent *dp;
	long loc;
	int count, i;

	//dprintf(STDERR_FILENO, "\n[file:%s,\tpath:%s]\n", piece, path);

	if(!(dir = opendir(path)))
		return 0;

	loc = telldir(dir);
	for(count=0,dp=readdir(dir); dp; dp=readdir(dir))
		if(strstr(dp->d_name, piece)==dp->d_name && strcmp(dp->d_name, ".") && strcmp(dp->d_name, ".."))
			count++;

	if(count){
		*files = (char**)realloc(*files&&nfile?(*files):NULL, sizeof(char*)*(nfile+count));
		seekdir(dir, loc);
		for(i=nfile, dp=readdir(dir); dp; dp=readdir(dir))
			if(strstr(dp->d_name, piece)==dp->d_name && strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")){
				if(dp->d_type==DT_DIR)
					strncat(dp->d_name, "/", 256);
				(*files)[i++]=strdup(dp->d_name);
			}
	}
	closedir(dir);

	return nfile+count;
}

int strmatch(char *a, char *b){
	int i;

	for(i=0; a[i]&&b[i]; i++)
		if(a[i]^b[i])	break;

	return i;
}
