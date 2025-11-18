/*
Most of the methods here were adapted from Vannyda Long's COMP30023-PROJECT 1
SEMESTER 1 2025.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dataStruct.h"

#define CACHE_CAPACITY 10
#define DEFAULT_HOST_PORT "80"

/*****************************************************************************/
// malloc and initialise a cache entry
cacheEntry_t *create_cache_entry() {
    cacheEntry_t *entry = calloc(1, sizeof(cacheEntry_t));
    assert(entry);
    entry->responseContentLength = 0;
    entry->next = NULL;
    entry->isCachable = 1;
    entry->maxAge = 0;
    entry->cachedTime = 0;
    entry->isStalable = 0;
    strcpy(entry->targetPort, DEFAULT_HOST_PORT);
    memset(entry->path, 0, sizeof(entry->path));
    memset(entry->host, 0, sizeof(entry->host));
    memset(entry->request_lastLine, 0, sizeof(entry->request_lastLine));
    memset(entry->request, 0, sizeof(entry->request));
    memset(entry->response, 0, sizeof(entry->response));
    return entry;
}

// malloc and initialise a cache linked list
cache_t *create_cache() {
    cache_t *cache = malloc(sizeof(cache_t));
    assert(cache);
    cache->head = cache->tail = NULL;
    cache->count = 0;
    return cache;
}

// free all malloced spaces
void free_cache(cache_t *cache) {
    if (!cache) {
        return;
    }
    cacheEntry_t *curr = cache->head;
    while (curr) {
        curr = curr->next;
        free(cache->head);
        cache->head = curr;
    }
    free(cache);
}

/**************************************************************************/
// least recently updated algorithm to update most recently accessed cache
void perform_lru(cache_t *cache, cacheEntry_t *newEntry, cacheEntry_t *isStale,
                 int *inCache) {

    if (!cache->head) {
        return;
    }
    cacheEntry_t *curr = cache->head;
    cacheEntry_t *prev = NULL;
    int staleCache = 0;

    // loop through cache linked list to find existing cached request
    while (curr) {
        if (strcmp(curr->request, newEntry->request) == 0) {
            // if cache is stale, enqueue this cache at the head
            if (isStale) {
                if (curr->next == NULL && curr != cache->head) {
                    prev->next = NULL;
                    cache->tail = prev;
                } else if (curr != cache->head) {
                    prev->next = curr->next;
                    curr->next = cache->head;
                    cache->head = curr;
                }
                staleCache = 1;

            } else {
                // if not stale, enqueue the matched cache at the tail
                if (curr == cache->head) {
                    cache->head = cache->head->next;
                } else if (curr->next != NULL) {
                    prev->next = curr->next;
                }
                *inCache = 1;
                if (cache->tail != curr) {
                    cache->tail->next = curr;
                    cache->tail = curr;
                    curr->next = NULL;
                }
                if (!cache->head) {
                    cache->head = cache->tail;
                }
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    // evict head of cache if count capacity is reached or cache is stale
    if ((!(*inCache) && cache->count == CACHE_CAPACITY) || staleCache) {
        cacheEntry_t *evicted = cache->head;
        cache->head = cache->head->next;
        printf("Evicting %s %s from cache\n", evicted->host, evicted->path);
        fflush(stdout);
        free(evicted);
        (cache->count)--;
        // removed stale cache
        if (staleCache) {
            *inCache = 0;
        }
    }
}

// enqueue new entry to cache linked list
void enqueue_cache(cache_t *cache, cacheEntry_t *newEntry) {
    if (!cache->head) {
        cache->head = cache->tail = newEntry;
        (cache->count)++;
        return;
    }
    // enqueue at tail
    cache->tail->next = newEntry;
    cache->tail = newEntry;
    (cache->count)++;
}

/**************************************************************************/
