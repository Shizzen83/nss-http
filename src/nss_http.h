#ifndef NSS_HTTP_H_INCLUDED
#define NSS_HTTP_H_INCLUDED

#define STRING_SIZE 512

#define NSS_HTTP_INITIAL_BUFFER_SIZE (256 * 1024)  /* 256 KB */
#define NSS_HTTP_MAX_BUFFER_SIZE (10 * 1024 * 1024)  /* 10 MB */

struct Response {
    size_t size;
    char *data;
};

typedef enum {
    RC_OK = 0,
    RC_FAIL_INIT_CURL = 100,
    RC_FAIL_BEARER_TOKEN = 101,
    RC_FAIL_REQUEST = 103,
    RC_WRONG_RESPONSE = 104
} ReturnCode;

ReturnCode nss_http_request(const char *, char **);

void handleReturnCode(ReturnCode);

size_t j_strlen(json_t *);

#endif
