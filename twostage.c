/* twostage v0.2
 * zsh@imipolexg.org 
 *
 * TODO: 
 *       * Add logging of failed passcode attempts. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <wait.h>

#include "config.h"
#include "trust.h"

#define MSG "Your twostage passcode is %d\n"
#define DEFAULT_SHELL "/bin/sh"

#define TRUST_STORE "trust"

#define LEN 1024

int executeur(char *shell, int argc, char **argv);
unsigned int stupid_random(void);
int send_passcode(config_t *cfg, unsigned int passcode);
int check_input(unsigned int passcode);

/* crazy globals, trix are for kids */
int executeur(char *shell, int argcount, char **argvee)
{
	char **args = (char **)malloc(sizeof(char *) * (argcount + 1));
	int i;

	args[0] = shell;
	for(i = 1 ; i < argcount ; i++) 
	{
		args[i] = argvee[i];
	}
	args[argcount] = NULL;

	return execv(shell, args);
}

unsigned int stupid_random(void)
{
	int urandom_fd;
	unsigned int randy = 0;

	urandom_fd = open("/dev/urandom", O_RDONLY);

	/* We want a number between 10^6 and 10^7 so that we get a 7 digit
	   number */
	while (!(randy > (1000000 - 1) && randy < (10000000 - 1)))
	{
		/* read 3 bytes intead of 4 to insure less hits over
		   the threshold (so we get our random number faster) */
		read(urandom_fd, &randy, sizeof(unsigned int)-1);
	}

	close(urandom_fd);

	return (unsigned int)randy;
}

#define STDINFD fileno(stdin)

/* ye ole' pipe-and-fork-and-exec */
int send_passcode(config_t *cfg, unsigned int passcode)
{
	int pid;
	int pipe_fds[2];

	if(pipe(pipe_fds) == -1)
		return -1;

	if((pid = fork()) == -1)
		return -1;
	else if(pid > 0) /* la mère */
	{ 
		char *s;
		size_t sz;
		int status;

		/* close read end */
		if(close(pipe_fds[0]) == -1)
			return -1; 
	
		/* -2, to erase %d, +7 for the passcode, +1 for \0 */	
		s = (char *) malloc(strlen(MSG)-2+7+1);
		if(!s)
			return -1;

		snprintf(s, strlen(MSG)-2+7+1, MSG, passcode);
		s[strlen(s)] = 0;
	
		/* MSG is so small that the write should almost alway 
                 * succeed unless something really bad is going on */	
		sz = strlen(s)+1;
		if (write(pipe_fds[1], s, sz) != sz)
		{
			free(s);
			return -1;	
		}

		/* we're done, close write end */
		if(close(pipe_fds[1]) == -1) 
		{
			free(s);
			return -1;
		}

		if(waitpid(pid, &status, 0) == -1)
			return -1;

		if(WIFEXITED(status))
		{
			if(WEXITSTATUS(status) == -1)
				return -1;
		}
	}
	else /* quand j'étais un petit enfant */ 
	{
		char *mail, *argv0;

		/* close write end */
		if(close(pipe_fds[1]) == -1)
			exit(-1);
		
		/* are there cooler ways to do this on new unices?
         * E.g., linux? FreeBSD? */
		if(pipe_fds[0] != STDINFD)
		{
			if(dup2(pipe_fds[0], STDINFD) != STDINFD)
				exit(-1);
			if(close(pipe_fds[0]) == -1)
				exit(-1);
		}

		mail = (char *)malloc(strlen(cfg_entry(cfg, MAIL))+1);
		if(!mail)
			exit(-1);
		strcpy(mail, cfg_entry(cfg, MAIL));

		if((argv0 = strrchr(mail, '/')) != NULL)
			argv0++;
		else
			argv0 = mail;
		
		if(execl(mail, argv0, "-s", "\"\"", cfg_entry(cfg, TO), 
			(char *)0) == -1)
		{
			exit(-1);
		}

		exit(0);
	}

	return 0;
}

#define INPUT_MAX 10
/* Run after passcode is generated. Returns 1 on success, 0 on failure */
int check_input(unsigned int passcode)
{
	char input[INPUT_MAX];
	unsigned int input_uint;
	
	if(fgets(input, INPUT_MAX, stdin) != NULL)
	{
		input_uint = (unsigned int)strtol(input, NULL, 10);
		if(input_uint == passcode)	
			return 1;
	}

	return 0;
}


/* Make sure the shell in the user cfg dir is a valid login shell */
int check_shell(char *shell)
{
	char *valid_shell;

	setusershell();
	while((valid_shell = getusershell()) != NULL)
	{
		if (strlen(valid_shell) != strlen(shell))
			continue;

		/* strcmp is bad, sure, but we also know that the strings
		   are the same length *and* 0 terminated */
		if(strcmp(shell, valid_shell) == 0)
			return 1;
	}
	endusershell();

	return 0;
}

#define TRIES_MAX 3

int we_should_trust(void)
{	
	char *client;
	char input[INPUT_MAX];

	client = get_client();
	if(!client)
		return 0;

	printf("\n\n");
	printf("If you trust this host, you can store it for a few days.\n");
	printf("Doing so will bypass this twostage authentication.\n");
	printf("This also makes scp and other things possible.\n");
	printf("If you don't trust this host but need scp, store it now,\n");
	printf("Then delete the entry in~/.twostage/trust/ later\n");
	printf("\nWould you like to trust %s for 15 days? [y/N] ", client);

	if(fgets(input, INPUT_MAX, stdin) == NULL)
		return 0;

	if(input[0] == 'y' || input[0] == 'Y')			
		return 1;
	else
		return 0;
}

trust_t *get_trusty()
{
		char *path;

#define TRTLN strlen(getenv("HOME"))+1+strlen(CFG_DIR)+1+strlen(TRUST_STORE)+1

		path = (char *)malloc(TRTLN);
		if(!path)
			return NULL;

		snprintf(path, TRTLN, "%s/%s/%s", getenv("HOME"), CFG_DIR,
				TRUST_STORE);	
		return open_trust_store(path);
}

int main(int argc, char *argv[])
{
	int tries;
	char *shell;
	trust_t *trust = NULL;
	config_t *cfg;
	unsigned int passcode;

	cfg = NULL;
	if((cfg = get_config()) == NULL)
	{
		perror("get_config");
		printf("twostage seems unconfigured! dropping to shell.\n");
		goto drop_to_shell;
	}

	trust = get_trusty();
	if(is_client_trusted(trust, get_client()) == 1)
	{
		printf("\n(twostage) Looks like we trust you. Cheers!\n");
		goto drop_to_shell;
	}

	printf("\n(twostage) Sending the passcode to your phone.\n");
	passcode = stupid_random();
	if(send_passcode(cfg, passcode) != 0)
	{
		printf("Oh no! Badness! Try again or contact the admin...\n");
		return -1;
	}	

	printf("Passcode sent! Have patience, then enter it: ");
	for(tries = 0 ; tries < TRIES_MAX ; tries++)
	{
		if(check_input(passcode))
			break;

		if(TRIES_MAX - tries == 1)
		{
			printf("Nice try punk. But we're onto you.\n");
			return -1;
		}
		else
			printf("Try again: ");
	}

	if(we_should_trust())
	{
		printf("Trusting...\n");
		if(trust_it(trust, get_client(), 15*24*60*60) == -1)
			perror("trust_it"); /* this is bad, but not fatal */
	}
	else
		printf("Not trusting...\n");

drop_to_shell:
	/* I suppose it's possible that this could fail... */
	chdir(getenv("HOME"));

	if(trust)
		close_trust_store(trust);

	if(cfg && check_shell(cfg[SHELL]))
		shell = cfg[SHELL];
	else
		shell = DEFAULT_SHELL;

	if(executeur(shell, argc, argv) == -1)
	{
		perror("execv");
		return -1;
	}

	return 0;
} 
