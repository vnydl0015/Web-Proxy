#ifndef SOCKETS
#define SOCKETS

#include "dataStruct.h"

// forward request to host
int forward_request(cacheEntry_t *cacheEntry);
// create a listening socket
int create_listening_socket(char *tcpPort, char *host);
// extract cache-control header
void validateCache(cacheEntry_t *entry, char *headerLine);
// check is cache is stale
cacheEntry_t *check_stale_cache(cache_t *cache, cacheEntry_t *newEntry);
// check if a substring exists in a longer string
int my_memmem(char *string, int stringlen, char *substring, int sublen);
// extract headers in both request and response
void extract_headers(cacheEntry_t *cacheEntry, int isRequest);
// read headers and body of both request and response
void read_message(cacheEntry_t *cacheEntry, int serverfd, int *cacheable,
                  int forwardToClient, int clientfd, int isHeader);
// get un-stale cache
void fetch_cache(cacheEntry_t *entry, int clientfd, cache_t *cache);

#endif