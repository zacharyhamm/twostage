#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "trust.h"

#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif /* S_IRWXU */

trust_t *open_trust_store(char *path)
{
    int fd;
    trust_t *trust_store;
    struct stat statbuf;

    if((fd = open(path, O_RDONLY, S_IRWXU)) == -1)
    {   
        if(errno != ENOENT) 
        {
            perror("open");
            return NULL;
        }

        if(mkdir(path, S_IRWXU) == -1)
        {
            perror("mkdir");
            return NULL;
        }
    }
    else
    {
        if(fstat(fd, &statbuf) == -1)
            return NULL;

        if(!S_ISDIR(statbuf.st_mode))
        {
            close(fd);
            errno = ENOTDIR;
            return NULL;
        }
    
        close(fd);
    }

    if((trust_store = (trust_t *) opendir(path)) == NULL)
    {
        perror("opendir");
        return NULL;
    }

    return trust_store;
}

void close_trust_store(trust_t *trust_store)
{
    closedir((DIR *)trust_store);
}

char *get_client(void)
{   
    char *client_env;
    static char *client = NULL;

    if(client)
        return client;

    client_env = getenv("SSH_CLIENT");
    if(!client_env)
        return NULL;
    client = malloc(strlen(client_env)+1);
    if(!client)
        return NULL;
    strcpy(client, client_env);

    /* XXX: should we do more input verification? to make sure we 
        really get an IP address here? */
    strtok(client, " ");

    return client;
}

/* FYI: 30 days is 30 * 24 * 60 * 60 seconds */
int trust_it(trust_t *trust_store, char *client, int ttl_seconds)
{
    int fd;
    char *path;
    char ttl_s[512];
    struct timeval tv;

    if(is_client_trusted(trust_store, client))
    {
        errno = EEXIST;
        return -1;
    }

    if(gettimeofday(&tv, NULL) == -1)
        return -1;
    
    if(snprintf(ttl_s, 512, "%ld", ttl_seconds+tv.tv_sec) < 0)
        return -1;

    path = malloc(strlen(client)+strlen(ttl_s)+2);
    if(!path)
        return -1;
    
    if(snprintf(path, strlen(client)+strlen(ttl_s)+2, "%s-%s", client,
            ttl_s) < 0)
    {
        free(path);
        return -1;
    }

    if(trust_chdir(trust_store) == -1)
    {
        free(path);
        return -1;
    }   

    /* Just "touch" it */   
    if((fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) == -1) 
    {
        free(path);
        return -1;
    }


    close(fd);
    free(path);

    return 0;
}

int is_client_trusted(trust_t *trust_store, char *client)
{
    struct dirent *dent;

    trust_chdir(trust_store);

    while((dent = readdir((DIR *)trust_store)) != NULL)
    {
        char *s = malloc(strlen(dent->d_name)+1);
        if(!s)
            return -1;  
    
        /* copy the name since strtok() messes with the string */   
        strncpy(s, dent->d_name, strlen(dent->d_name)+1);
        strtok(s, "-");     
        
        if(strlen(s) != strlen(client))
        {
            free(s);
            continue;
        }

        if(strcmp(s, client) == 0)
        {
            struct timeval tv;
            char *time_s;
            long int time_li;

            time_s = strtok(NULL, "-");
            if(!time_s)
            {
                free(s);
                return -1;
            }

            if(gettimeofday(&tv, NULL) == -1)
            {
                free(s);
                return -1;
            }

            time_li = strtol(time_s, NULL, 10);
            if(time_li > 0 && time_li > tv.tv_sec)
            {
                free(s);
                return 1; 
            }
            else if(time_li > 0 && time_li <= tv.tv_sec)
            {
                /* trust has expired */
                if(unlink(dent->d_name) == -1)
                {
                    return -1;
                }
                
                /* don't return 0 here (there might be another
                 * trust entry for the same client) */
            }
        }   
        free(s);
    }
        
    return 0;   
}

int trust_chdir(trust_t *trust_store)
{
    int fd;

    if((fd = dirfd((DIR *)trust_store)) == -1)
        return -1;

    if(fchdir(fd) == -1)
        return -1;

    return 0;
}

/*
#define CFG_DIR ".twostage"
#define TRUST_STORE "trust"

int main(int argc, char *argv[])
{
    trust_t *trust_store;
    char *path;

#define TRTLN strlen(getenv("HOME"))+1+strlen(CFG_DIR)+1+strlen(TRUST_STORE)+1

    path = (char *)malloc(TRTLN);
    if(!path)
        return -1;

    snprintf(path, TRTLN, "%s/%s/%s", getenv("HOME"), CFG_DIR, TRUST_STORE);    
    printf("%s\n", path);
    trust_store = open_trust_store(path);
    if(!trust_store)
    {
        perror("open_trust_store");
        return -1;
    }

    if(trust_it(trust_store, argv[1], 15*24*60*60) == -1)
    {
        perror("trust_it");
        return -1;
    }
    close_trust_store(trust_store);

    return 0;
}
*/
