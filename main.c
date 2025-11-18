#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>

#include "dataStruct.h"
#include "sockets.h"

#define BACKLOG 10
#define MALFORMED_REQUEST 10
#define DEFAULT_LISTEN_PORT "8080"

/**************************************************************************/
void perform_caching_stages(cache_t *cache, cacheEntry_t *newCacheEntry,
                            int *inCache, int *cacheable,
                            cacheEntry_t *isStale);
void get_port(int argc, char **argv, char **tcpPort, int *stage2);
void evict_stale_cache(cacheEntry_t *isStale, cache_t *cache,
                       cacheEntry_t *entry, int *inCache);
/**************************************************************************/

int main(int argc, char *argv[]) {

    char *tcpPort = DEFAULT_LISTEN_PORT;
    int stage2 = 0;
    cache_t *cache = create_cache();
    get_port(argc, argv, &tcpPort, &stage2);

    // create a listening socket
    int listenfd = create_listening_socket(tcpPort, NULL);
    int clientfd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;

    // Accept a connection - loop until CTRL-C
    while (1) {
        // get a valid client address
        client_addr_size = sizeof(client_addr);
        clientfd = accept(listenfd, (struct sockaddr *)&client_addr,
                          &client_addr_size);
        if (clientfd < 0) {
            continue;
        }
        printf("Accepted\n");
        fflush(stdout);

        // store new request in new cache entry
        cacheEntry_t *newCacheEntry = create_cache_entry();
        int cacheable = 1, inCache = 0;
        read_message(newCacheEntry, clientfd, &cacheable, 1, -1, 1);

        // handle malformed request
        if (newCacheEntry->requestLength < MALFORMED_REQUEST ||
            newCacheEntry->path[0] == '\0') {
            close(clientfd);
            continue;
        }

        // checking for any stale cache
        cacheEntry_t *isStale = check_stale_cache(cache, newCacheEntry);

        // perform least recently updated algorithm on cache
        if (stage2 && cacheable) {
            perform_lru(cache, newCacheEntry, NULL, &inCache);
        }

        // fetch cache that is not stale
        if (inCache && !isStale) {
            fetch_cache(newCacheEntry, clientfd, cache);
        } else {
            // if not in cache, forward to host server normally
            int originfd = forward_request(newCacheEntry);
            read_message(newCacheEntry, originfd, &cacheable, 0, clientfd, 1);
            if (stage2) {
                perform_caching_stages(cache, newCacheEntry, &inCache,
                                       &cacheable, isStale);
            } else {
                free(newCacheEntry);
            }
        }
        close(clientfd);
    }
    free_cache(cache);
    return 0;
}

/**************************************************************************/
// evict any stale 'now' un-cacheable requestQ
void evict_stale_cache(cacheEntry_t *isStale, cache_t *cache,
                       cacheEntry_t *entry, int *inCache) {
    if (isStale) {
        perform_lru(cache, entry, isStale, inCache);
    }
}

// perform all tasks from 2-4
void perform_caching_stages(cache_t *cache, cacheEntry_t *newCacheEntry,
                            int *inCache, int *cacheable,
                            cacheEntry_t *isStale) {
    // new response is not cacheable (based on byte size)
    if (!(*cacheable)) {
        evict_stale_cache(isStale, cache, newCacheEntry, inCache);
        free(newCacheEntry);
        return;
    }
    // new response is within byte size, but cache-control says no
    if (!(newCacheEntry->isCachable)) {
        printf("Not caching %s %s\n", newCacheEntry->host, newCacheEntry->path);
        fflush(stdout);
        evict_stale_cache(isStale, cache, newCacheEntry, inCache);
        free(newCacheEntry);
        return;
    }
    // if this new response is not previously stale
    if (!isStale) {
        newCacheEntry->cachedTime = time(NULL);
        enqueue_cache(cache, newCacheEntry);
        return;
    }
    // if this new response is previously stale, copy new response to this
    // stale cache
    memcpy(isStale, newCacheEntry, sizeof(cacheEntry_t));
    free(newCacheEntry);
}

// get listening port number
void get_port(int argc, char **argv, char **tcpPort, int *stage2) {
    // get tcp port number and cache flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            *tcpPort = argv[i + 1];
        } else if (strcmp("-c", argv[i]) == 0) {
            *stage2 = 1;
        }
    }
}

/**************************************************************************/
