/**********************************************************************/

/* Acknowledgement of external codes usage. Most of the following methods are
obtained from various online sources such as:

- Week 8's workshop for create_listening_socket()
- https://man7.org/linux/man-pages/man3/getaddrinfo.3.html for listening
    socket's connection
- Adapting memmem() from <stddef.h> to check if a substring exists in longer
    string https://www.capitalware.com/rl_blog/?p=5847
- memcpy from https://www.geeksforgeeks.org/memcpy-in-cc/
- strtok from
https://www.geeksforgeeks.org/strtok-strtok_r-functions-c-examples/
- strncasecmp from
https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-strncasecmp-case-insensitive-string-comparison
*/

/**********************************************************************/

#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "sockets.h"

#define BACKLOG 10
#define MAX_BYTE 102400
#define BUFFER_SIZE 4096
#define MAX_REQUEST_LENGTH 2000
#define MALFORMED 10

#define MALFORMED_END "\n\n\n"
#define EMPTY_LINE "\r\n\r\n"
#define HEADER_END "\r\n"
#define GET "GET"
#define HOST "Host:"
#define CONTENT "Content-Length:"
#define CACHE "Cache-Control:"

/**********************************************************************/

// create a listening socket to all interfaces (including ipv4 and ipv6)
// adapted from Workshop 8 (week 8) and
// https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
// host == NULL for client and host != NULL for domain server
int create_listening_socket(char *tcpPort, char *host) {

    int listenfd = 0, enable = 1;
    struct addrinfo hints, *res, *ptr;

    // Create address we're going to listen on
    memset(&hints, 0, sizeof hints);
    if (host == NULL) {
        hints.ai_family = AF_INET6; // since VM disables IPV6_ONLY, Ed#909
        hints.ai_flags = AI_PASSIVE;
    } else {
        hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    }
    hints.ai_socktype = SOCK_STREAM;

    // NULL means any interface, service (port)
    if (getaddrinfo(host, tcpPort, &hints, &res) != 0) {
        perror("Error: Cannot get address of server\n");
        exit(EXIT_FAILURE);
    }

    // attempt to bind to each address until succeeded
    for (ptr = res; ptr != NULL; ptr = ptr->ai_next) {
        listenfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (listenfd < 0) {
            continue;
        }

        // listening socket to client
        if (host == NULL) {
            // Reuse port if possible and try to bind address to socket
            if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable,
                           sizeof(int)) < 0 ||
                bind(listenfd, ptr->ai_addr, ptr->ai_addrlen) < 0) {
                close(listenfd);
                continue;
            }
        } else if (connect(listenfd, ptr->ai_addr, ptr->ai_addrlen) < 0) {
            // listening socket to host origin
            close(listenfd);
            continue;
        }

        break;
    }
    freeaddrinfo(res);

    // accepting client connections
    if (host == NULL && listen(listenfd, BACKLOG) < 0) {
        perror("Error: Failed to listen for any connection\n");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    return listenfd;
}

// read client's request or read host's response then send back to client
// forwardRequest = 1 means sending to host server instead of client
// isHeader = 1 means it is reading header instead of body
void read_message(cacheEntry_t *cacheEntry, int serverfd, int *cacheable,
                  int forwardRequest, int clientfd, int isHeader) {

    // initialise for different tasks: sending header or body, to host or client
    char *line =
        (forwardRequest == 1) ? cacheEntry->request : cacheEntry->response;
    char buffer[BUFFER_SIZE];
    int totalBytes = 0, bytesRead, remainingBytes = 1;
    // if reading body of host's response
    if (isHeader == 0) {
        remainingBytes = cacheEntry->responseContentLength -
                         cacheEntry->responseTotalBytes +
                         cacheEntry->responseHeaderLength;
        totalBytes = cacheEntry->responseTotalBytes;
    }

    // Read messages and store them in cache entry. Adapted from
    // https://gamedev.net/forums/topic/651197-calling-recv-in-a-loop/
    while (remainingBytes > 0) {
        bytesRead = recv(serverfd, buffer, BUFFER_SIZE, 0);
        // errors while reading
        if (bytesRead <= 0 || (forwardRequest == 0 &&
                               (send(clientfd, buffer, bytesRead, 0) < 0))) {
            break;
        }
        // store messages in cache within max bytes
        if (totalBytes + bytesRead <= MAX_BYTE) {
            memcpy(line + totalBytes, buffer, bytesRead);
        } else {
            *cacheable = 0;
        }
        totalBytes += bytesRead;
        // track the ending of either a header or a body
        if (isHeader == 1) {
            // handle malformed header
            if (strstr(line, MALFORMED_END) && totalBytes < MALFORMED) {
                cacheEntry->requestLength = totalBytes;
                return;
            }
            cacheEntry->responseHeaderLength =
                my_memmem(line, totalBytes, EMPTY_LINE, strlen(EMPTY_LINE));
            if (cacheEntry->responseHeaderLength > -1) {
                break;
            }
        } else {
            remainingBytes -= bytesRead;
        }
    }

    // extract header for request
    if (forwardRequest == 1) {
        cacheEntry->requestLength = totalBytes;
        if (cacheEntry->requestLength >= MAX_REQUEST_LENGTH) {
            *cacheable = 0;
        }
        extract_headers(cacheEntry, 1);
        return;
    }
    // extract header for response and print out body length
    cacheEntry->responseTotalBytes = totalBytes;
    if (isHeader == 0) {
        return;
    }
    extract_headers(cacheEntry, 0);
    read_message(cacheEntry, serverfd, cacheable, 0, clientfd, 0);
    printf("Response body length %d\n", cacheEntry->responseContentLength);
    fflush(stdout);
}

// extract headers in both request and response
void extract_headers(cacheEntry_t *cacheEntry, int isRequest) {

    char headerLines[MAX_BYTE];
    char *document = NULL;
    int length = -1;
    // create a copy of either a request or a response
    if (isRequest == 1) {
        document = cacheEntry->request;
        length = cacheEntry->requestLength;
    } else {
        document = cacheEntry->response;
        length = cacheEntry->responseHeaderLength;
    }
    memcpy(headerLines, document, length);
    headerLines[length] = '\0';

    // extracting relevant data
    char *line = strtok(headerLines, HEADER_END);
    char *lastLineptr = NULL;
    while (line) {
        lastLineptr = line;
        if (strncasecmp(line, GET, strlen(GET)) == 0) {
            sscanf(line + strlen(GET), " %s", cacheEntry->path);
        }
        if (strncasecmp(line, HOST, strlen(HOST)) == 0) {
            sscanf(line + strlen(HOST), " %[^:]:%s", cacheEntry->host,
                   cacheEntry->targetPort);
        }
        if (strncasecmp(line, CONTENT, strlen(CONTENT)) == 0) {
            sscanf(line + strlen(CONTENT), " %d",
                   &cacheEntry->responseContentLength);
        }
        if (strncasecmp(line, CACHE, strlen(CACHE)) == 0) {
            validateCache(cacheEntry, line);
        }
        // continue looping through each line
        line = strtok(NULL, HEADER_END);
    }

    if (!isRequest) {
        return;
    }
    // extracting the last line of a request
    memcpy(cacheEntry->request_lastLine, lastLineptr, strlen(lastLineptr) + 1);
    printf("Request tail %s\n", cacheEntry->request_lastLine);
    fflush(stdout);
}

// Function to forward request to target host and get response
int forward_request(cacheEntry_t *cacheEntry) {
    int originfd =
        create_listening_socket(cacheEntry->targetPort, cacheEntry->host);
    // Send the request to host origin server
    if (send(originfd, cacheEntry->request, cacheEntry->requestLength, 0) < 0) {
        perror("Error: Failed to send to host\n");
        close(originfd);
        exit(EXIT_FAILURE);
    }
    // output result to stdout
    printf("GETting %s %s\n", cacheEntry->host, cacheEntry->path);
    fflush(stdout);
    return originfd;
}

/*****************************************************************************/

// check if cache is stale
cacheEntry_t *check_stale_cache(cache_t *cache, cacheEntry_t *newEntry) {
    if (!cache->head) {
        return NULL;
    }
    cacheEntry_t *curr = cache->head;
    while (curr) {
        if (strcmp(curr->request, newEntry->request) == 0 && curr->isStalable) {
            if (time(NULL) - curr->cachedTime >= curr->maxAge) {
                printf("Stale entry for %s %s\n", curr->host, curr->path);
                fflush(stdout);
                return curr;
            }
        }
        curr = curr->next;
    }
    return NULL;
}

// extract cache-control header
// strstr from
// https://www.tutorialspoint.com/c_standard_library/c_function_strstr.htm
void validateCache(cacheEntry_t *entry, char *headerLine) {
    char *cacheControlDirectory[] = {"private",         "no-store",
                                     "no-cache",        "max-age=0",
                                     "must-revalidate", "proxy-revalidate"};
    int size = 6;
    // lower headerLine
    for (int i = 0; headerLine[i]; i++) {
        headerLine[i] = tolower(headerLine[i]);
    }
    // extract max-age
    char *maxAge = strstr(headerLine, "max-age=");
    if (maxAge) {
        sscanf(maxAge, "max-age= %d", &entry->maxAge);
        if (entry->maxAge != 0) {
            entry->isStalable = 1;
        }
    }
    // not cachable
    for (int i = 0; i < size; i++) {
        if (strstr(headerLine, cacheControlDirectory[i])) {
            entry->isCachable = 0;
            entry->isStalable = 0;
            entry->maxAge = 0;
            return;
        }
    }
}

// get un-stale cache
void fetch_cache(cacheEntry_t *entry, int clientfd, cache_t *cache) {
    printf("Serving %s %s from cache\n", entry->host, entry->path);
    fflush(stdout);
    send(clientfd, cache->tail->response, cache->tail->responseTotalBytes, 0);
    free(entry);
}

/*****************************************************************************/

// Adapting memmem() from <stddef.h> to check if substring exists in string
// https://www.capitalware.com/rl_blog/?p=5847
int my_memmem(char *string, int stringlen, char *substring, int sublen) {
    for (int offset = 0; offset <= stringlen - sublen; offset++) {
        if (memcmp(string + offset, substring, sublen) == 0) {
            return offset;
        }
    }
    return -1;
}

/*****************************************************************************/
