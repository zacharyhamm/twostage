#ifndef _CONFIG_H_
#define _CONFIG_H_

/* cfg indices */
#define FROM 0
#define TO 1
#define SMTP 2
#define SHELL 3
#define CFG_SZ 4

#define CFG_DIR ".twostage"
#define CFG_FN "twostage.cfg"
#define CFG_ENTRIES 4

#define LEN 1024

char *read_cfg_line(FILE *cfg_fp);
char **get_config(void);

#endif /* _CONFIG_H_ */
