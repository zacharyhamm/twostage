#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MAGIC "magic"

DIR *open_trust_store()
{
	char *home;
	int fd;
	DIR *magic;
	struct stat statbuf;

	home = getenv("HOME");
	chdir(home);
#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif /* S_IRWXU */
	if((fd = open(MAGIC, O_RDONLY, S_IRWXU)) == -1)
	{	
		if(errno != ENOENT)	
		{
			perror("open");
			return NULL;
		}

		/* directory does not exist */
		if(mkdir(MAGIC, S_IRWXU) == -1)
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

	if((magic = opendir(MAGIC)) == NULL)
	{
		perror("opendir");
		return NULL;
	}

	return magic;
}

int main(int argc, char *argv[])
{
	DIR *trust_dir;

	trust_dir = open_trust_store();
	if(trust_dir == NULL)
	{
		perror("open_trust_store");
		return -1;
	}
	
	return 0;	
}
