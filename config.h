#ifndef _CONFIG_H_
#define _CONFIG_H_


#define CFG_SZ 3

/* cfg indices */
#define TO 0
#define MAIL 1
#define SHELL 2

#define CFG_DIR ".twostage"
#define CFG_FN "twostage.cfg"
#define CFG_ENTRIES 4

#define LEN 1024

typedef char * config_t;

char *read_cfg_line(FILE *cfg_fp);
config_t *get_config(void);
char *cfg_entry(config_t *cfg, int entry);

#endif /* _CONFIG_H_ */
