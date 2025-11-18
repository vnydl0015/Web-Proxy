#ifndef DATASTRUCT
#define DATASTRUCT

#include <time.h>

#define BUFFER_SIZE 4096
#define MAX_RESPONSE_BUFFER 102400
#define MAX_REQUEST_BUFFER 8193 // Ed #200

// cache entry stores both request and response and most of their headers
typedef struct cacheEntry cacheEntry_t;
struct cacheEntry {
    // for request
    char path[MAX_REQUEST_BUFFER];
    char host[MAX_REQUEST_BUFFER];
    char request_lastLine[MAX_REQUEST_BUFFER];
    char request[MAX_REQUEST_BUFFER];
    int requestLength;
    char targetPort[BUFFER_SIZE];

    // for response
    char response[MAX_RESPONSE_BUFFER];
    int responseContentLength;
    int responseHeaderLength;
    int responseTotalBytes;

    // for tasks 3-4
    int isCachable;
    time_t cachedTime;
    unsigned int maxAge;
    int isStalable;

    cacheEntry_t *next;
};

// storing all caches to perform least recently updated algorithm
typedef struct cache cache_t;
struct cache {
    cacheEntry_t *head;
    cacheEntry_t *tail;
    int count;
};

// malloc and initialise a cache entry
cacheEntry_t *create_cache_entry();
// malloc and initialise a cache linked list
cache_t *create_cache();
// enqueue new entry to cache linked list
void enqueue_cache(cache_t *cache, cacheEntry_t *newEntry);
// least recently updated algorithm to update most recently accessed cache
void perform_lru(cache_t *cache, cacheEntry_t *newEntry, cacheEntry_t *isStale,
                 int *inCache);
// free all malloced
void free_cache(cache_t *cache);

#endif