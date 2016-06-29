#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include "include/complete.h"

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

	if(count==1){
		if(p = strrchr(line, '/'))	p++;
		else				p = line;

		strncpy(p, files[0], line_size-(p-line));
		free(files[0]);
		free(files);
		return true;
	}
	else if(count){
		int i;

		for(i=0; i<count; i++){
			dprintf(STDERR_FILENO, i%5 ? "%-32s" : "\n%-32s", files[i]);
			free(files[i]);
		}
		dprintf(STDERR_FILENO, "\n");
		free(files);
	}

	return false;
}

int candidate(char **files[], int nfile, char *path, char *piece){
	DIR *dir;
	struct dirent *dp;
	long loc;
	int count, i;

	//dprintf(STDERR_FILENO, "[file:%s,\tpath:%s]\n", piece, path);

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
