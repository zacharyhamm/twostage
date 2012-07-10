#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"

char *cfg_entry(config_t *cfg, int entry)
{
    if(entry >= CFG_SZ || entry < 0)
        return NULL;
    return cfg[entry];
}

/* 
 * Mostly duplicates getline() semantics across BSD and Linux
 * Mostly, not entirely... 
 */
ssize_t read_a_line(char **line, size_t *n, FILE *fp)
{
#ifdef _POSIX_C_SOURCE >= 200809L
    return getline(line, n, fp);
#elif _BSD_
    char *buf, *lbuf;
    size_t len;

    lbuf = NULL;
    if((buf = fgetln(fp, &len)) == NULL)
        return -1;

    if(buf[len-1] == '\n')
        buf[len-1] = 0;
    else
    {
        if((lbuf = malloc(len+1)) == NULL)
        {
            free(buf);
            return -1;
        }

        memcpy(lbuf, buf, len);
        lbuf[len] = 0;
        buf = lbuf;
    }

    if(*line == NULL)
    {
        if(lbuf)
        {
            *line = lbuf;
            return (ssize_t)len;
        }
        else
        {
            *line = (char *)malloc(len);
            if(*line == NULL)
                return -1;
            memcpy(*line, buf, len);
            return (ssize_t)(len-1);
        }
    }
    else
    {
        /* XXX we should realloc here, but in the meantime... */
        if(len > *n)
        {
            if(lbuf)
                free(lbuf);
            return -1;
        }

        if(lbuf)
        {
            *line = lbuf;
            return (ssize_t)(len-1);
        }
        else
        {
            if(*n > len)
                memcpy(*line, buf, len);
            else
                memcpy(*line, buf, *n);

            return (ssize_t)len;
        }
    }
#endif /* _BSD_ */
}

char *read_cfg_line(FILE *cfg_fp)
{
    char *line = (char *)malloc(LEN);
    size_t sz;

    if(!line) return NULL;

    sz = LEN;

    if(read_a_line(&line, &sz, cfg_fp) == -1)
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

    /* The config file is just three simple lines. */
    /* We trust that you didn't fuck it up */
    cfg = (char **) malloc(sizeof(char *) * CFG_SZ);
    if(!cfg)
        return NULL;

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
    printf("%s\n", cfg[TO]);
    printf("%s\n", cfg[MAIL]);
    printf("%s\n", cfg[SHELL]);
#endif /* DEBUG */
    
    return cfg;
}


