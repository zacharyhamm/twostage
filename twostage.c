/* twostage v0.2
 * ---
 * Zachary Hamm
 * zsh@imipolexg.org 
 *
 * TODO: 
 *       * Option to remember trusted hosts for 30 days so you don't
 * 		have to do this over and over from your main boxen.
 *       * Add logging of failed passcode attempts. 
 * 	 * Does this break X11 forwarding? Must test. SCP?
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
#include <db.h>
#include <curl/curl.h>

#define MSG "From: %s\r\nTo: %s\r\n\r\nYour two-stage passcode is %d\r\n"
#define DEFAULT_SHELL "/bin/sh"

/* cfg indices */
#define FROM 0
#define TO 1
#define SMTP 2
#define SHELL 3
#define CFG_SZ 4

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
		/* 7 is the digit length of the ints from stupid_random() 
		   XXX: we assume malloc succeeds. this is sloppy. */
		s = (char *) malloc(strlen(MSG) + strlen(conf[FROM]) + 
					strlen(conf[TO]) + 7 + 1);
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

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

	smtp = (char *)malloc(strlen("smtp://") + strlen(conf[SMTP]) + 1);
	if(!smtp)
	{	
		curl_easy_cleanup(curl);
		return -1;
	}

	snprintf(smtp, strlen("smtp://") + strlen(conf[SMTP]) + 1,
			"smtp://%s", conf[SMTP]);
	printf("%s\n", smtp);
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

#define CFG_DIR ".twostage"
#define CFG_FN "twostage.cfg"
#define CFG_ENTRIES 4

#define LEN 1024

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

	printf("%s\n", cfg[FROM]);
	printf("%s\n", cfg[TO]);
	printf("%s\n", cfg[SMTP]);
	printf("%s\n", cfg[SHELL]);
	
	return cfg;
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

char *get_client(void)
{
	char *client;

	client = getenv("SSH_CLIENT");
	if(!client)
		return NULL;

	/* XXX: should we do more input verification? to make sure we 
		really get an IP address here? */
	strtok(client, " ");

	return client;
}

int is_client_trusted(void)
{
	char *ip;

	ip = get_client();
	if(!ip)
		return 0;

	/* implement! */

	return 0;
}

/*
void ask_about_client(void)
{	
	char *client;
	char input[INPUT_MAX];

	client = getenv("SSH_CLIENT");
	if(!client)
		return;

	strtok(client, " ");
	printf("Would you like to trust %s for 15 days? [y/N] ");

	if(fgets(input, INPUT_MAX, stdin) == NULL)
		return;

	if(input[0] == 'y' || input[0] == 'Y')			
}
*/

int main(int argc, char *argv[])
{
	int tries;
	char *shell;

	if((conf = get_config()) == NULL)
	{
		perror("get_config");
		return -1;
	}
	printf("%s\n", conf[SMTP]);

	if(is_client_trusted())
		goto drop_to_shell;
	
	printf("2stage! Sending the passcode to your phone.\n");
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


	/* TODO: Add client-trust question here */

drop_to_shell:

	if(check_shell(conf[SHELL]))
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
