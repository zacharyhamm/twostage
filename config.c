#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"

char *read_cfg_line(FILE *cfg_fp)
{
	char *line = (char *)malloc(LEN);
	size_t sz;

	if(!line) return NULL;

	sz = LEN;

	if(getline(&line, &sz, cfg_fp) == -1)
	{
		free(line);
		return NULL;
	}

	if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = 0;

	return line;
}


char **get_config(void)
{
	FILE *cfg_fp;
	size_t cfg_path_sz;
	char *cfg_path, *home;
	char **cfg;
	int fd, i;

	if((home = getenv("HOME")) == NULL)
		return NULL;

	/* 2x +1 for / and +1 for \0 */
	cfg_path_sz = strlen(home)+1+strlen(CFG_DIR)+1+strlen(CFG_FN)+1;

	cfg_path = (char *)malloc(cfg_path_sz);
	if(cfg_path == NULL)
		return NULL;

	snprintf(cfg_path, cfg_path_sz, "%s/%s/%s", home, CFG_DIR, CFG_FN);

	/* See if we have a config file */
	if((fd = open(cfg_path, O_RDONLY)) == -1)
	{
		free(cfg_path);
		return NULL;
	}
	free(cfg_path);

	cfg_fp = fdopen(fd, "r");

	/* The config file is just four simple lines. */
	/* We trust that you didn't fuck it up */
	cfg = (char **) malloc(sizeof(char *) * CFG_SZ);
	if(!cfg) return NULL;

	for(i = 0; i < CFG_SZ ; i++)
	{
		if((cfg[i] = read_cfg_line(cfg_fp)) == NULL)
		{
			int j;
			for(j = 0 ; j < i ; j++)
				free(cfg[j]);
		
			return NULL;
		}
	}

#ifdef DEBUG
	printf("%s\n", cfg[FROM]);
	printf("%s\n", cfg[TO]);
	printf("%s\n", cfg[SMTP]);
	printf("%s\n", cfg[SHELL]);
#endif /* DEBUG */
	
	return cfg;
}


