#ifndef _TRUST_H_
#define _TRUST_H_

#include <dirent.h>

typedef DIR trust_t;

trust_t *open_trust_store(char *store_name);
void close_trust_store(trust_t *trust_store);
int trust_it(trust_t *trust_store, char *client, int ttl_seconds);
int is_client_trusted(trust_t *trust_store, char *client);
char *get_client(void);
char *trust_chdir(trust_t *trust_store);

#endif /* _TRUST_H_ */
