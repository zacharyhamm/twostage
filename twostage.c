/* twostage v0.2
 * ---
 * Zachary Hamm
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
#include <curl/curl.h>

#include "config.h"
#include "trust.h"

#define MSG "From: %s\r\nTo: %s\r\n\r\nYour twostage passcode is %d\r\n"
#define DEFAULT_SHELL "/bin/sh"

#define TRUST_STORE "trust"

#define LEN 1024

int executeur(char *shell, int argc, char **argv);
unsigned int stupid_random(void);
size_t mailbody(void *ptr, size_t size, size_t nitems, void *strm);
int send_passcode(void);
char *read_cfg_line(FILE *cfg_fp);
char **get_config(void);

/* crazy globals, trix are for kids */
static char **conf;
static unsigned int passcode;

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

/* Callback for CURLOPT_READFUNCTION */
size_t mailbody(void *ptr, size_t size, size_t nitems, void *strm)
{
	static unsigned int dirty = 0;
	char *s;
	size_t len = 0;

	if(!dirty)
	{
		/* 7 is the digit length of the ints from stupid_random() */ 
		s = (char *) malloc(strlen(MSG) + strlen(conf[FROM]) + 
					strlen(conf[TO]) + 7 + 1);
		if(!s)
			return -1;

		passcode = stupid_random();
		sprintf(s, MSG, conf[FROM], conf[TO], passcode);
		len = strlen(s);
		memcpy(ptr, s, len);
		dirty = 1;
		free(s);

		return len; 
	}

	return 0;
}

int send_passcode(void)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *targets = NULL;
	char *smtp;
	
	curl = curl_easy_init();
	if (!curl) {
		printf("Something bad happened to curl_easy_init()!");
		return -1;
	}

#ifdef DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif /* DEBUG */

	smtp = (char *)malloc(strlen("smtp://") + strlen(conf[SMTP]) + 1);
	if(!smtp)
	{	
		curl_easy_cleanup(curl);
		return -1;
	}

	snprintf(smtp, strlen("smtp://") + strlen(conf[SMTP]) + 1,
			"smtp://%s", conf[SMTP]);
	curl_easy_setopt(curl, CURLOPT_URL, smtp);
	targets = curl_slist_append(targets, conf[TO]);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, targets);
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, conf[FROM]);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, mailbody);
	
	res = curl_easy_perform(curl);
	if(res != 0) 
	{
		printf("Something bad happened!\n");
		curl_easy_cleanup(curl);
		return -1;
	}

	curl_slist_free_all(targets);
	curl_easy_cleanup(curl);

	return 0;
}

#define INPUT_MAX 10
/* Run after passcode is generated. Returns 1 on success, 0 on failure */
int check_input(void)
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

	printf("Would you like to trust %s for 15 days? [y/N] ", client);
	printf("Don't do so if you don't trust the host!\nDoing so will make scp and other good things possible.\n");

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

	conf = NULL;
	if((conf = get_config()) == NULL)
	{
		perror("get_config");
		printf("twostage seems unconfigured! dropping to shell.\n");
		goto drop_to_shell;
	}

	trust = get_trusty();
	if(is_client_trusted(trust, get_client()) == 1)
		goto drop_to_shell;
	
	printf("(twostage) Sending the passcode to your phone.\n");
	if(send_passcode() != 0)
	{
		printf("Oh no! Badness! Try again or contact the admin...\n");
		return -1;
	}	

	printf("Passcode sent! Have patience, then enter it: ");
	for(tries = 0 ; tries < TRIES_MAX ; tries++)
	{
		if(check_input())
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
		if(trust_it(trust, get_client(), 15*24*60*60) == -1)
			perror("trust_it"); /* this is bad, but not fatal */
	}

drop_to_shell:
	/* I suppose it's possible that this could fail... */
	chdir(getenv("HOME"));

	if(trust)
		close_trust_store(trust);

	if(conf && check_shell(conf[SHELL]))
		shell = conf[SHELL];
	else
		shell = DEFAULT_SHELL;

	if(executeur(shell, argc, argv) == -1)
	{
		perror("execv");
		return -1;
	}

	return 0;
} 
