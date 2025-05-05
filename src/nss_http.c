#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include "nss_http.h"




// Newer versions of Jansson have this but the version
// on Ubuntu 12.04 don't, so make a wrapper.
size_t j_strlen(json_t *str)
{
    return strlen(json_string_value(str));
}


static size_t response_callback(
    void *content,
    size_t size,
    size_t nmemb,
    struct Response *result
)
{
    size_t realsize = size * nmemb;

    result->data = realloc(result->data, result->size + realsize + 1);
    if (!result->data) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        exit(EXIT_FAILURE);
    }

    memcpy(result->data + result->size, content, realsize);
    result->size += realsize;
    result->data[result->size] = '\0';

    return realsize;
}

static ReturnCode setupHeaders(struct curl_slist **headers)
{
    char authHeader[STRING_SIZE];

    snprintf(authHeader, STRING_SIZE, "Authorization: Bearer %s", getenv("AUTH_TOKEN"));

    *headers = curl_slist_append(*headers, authHeader);

    if (! *headers) {
        return RC_FAIL_BEARER_TOKEN;
    }

    *headers = curl_slist_append(*headers, "User-Agent: NSS-HTTP");
    *headers = curl_slist_append(*headers, "Accept: application/json");

    return RC_OK;
}


static ReturnCode setupCurl(
    CURL **curl,
    struct curl_slist **headers
)
{
    ReturnCode code;

    *curl = curl_easy_init();

    if (! *curl) {
        return RC_FAIL_INIT_CURL;
    }

    code = setupHeaders(headers);

    if (code != RC_OK) {
        return code;
    }

    curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, *headers);

    curl_easy_setopt(*curl, CURLOPT_CAINFO, NULL);

    //if (getenv("CACERT_PATH") == NULL) {
        curl_easy_setopt(*curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(*curl, CURLOPT_SSL_VERIFYHOST, 0L);
    //}

    return RC_OK;
}

static ReturnCode clearRequest(
    CURL *curl,
    struct curl_slist **headers
)
{
    if (*headers != NULL) {
        curl_slist_free_all(*headers);
    }

    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }

    return RC_OK;
}


ReturnCode nss_http_request(
    const char *url,
    char **data
)
{
    ReturnCode rCode;
    CURL *curl = NULL;
    CURLcode curlStatus;
    struct curl_slist *headers = NULL;
    long code;

    rCode = setupCurl(&curl, &headers);

    if (rCode != RC_OK) {
        clearRequest(curl, &headers);
        return rCode;
    }

    /**data = malloc(NSS_HTTP_INITIAL_BUFFER_SIZE);
    if (! *data) {
        fprintf(stderr, "malloc() failed!\n");
        exit(EXIT_FAILURE);
    }*/

    struct Response write_result;
    write_result.size = 0;
    write_result.data = malloc(write_result.size + 1);
    if (!write_result.data) {
        fprintf(stderr, "write_result malloc failed!\n");
        exit(EXIT_FAILURE);
    }
    write_result.data[0] = '\0';

    fprintf(stderr, "URL: %s\n", url);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    curlStatus = curl_easy_perform(curl);
    if(curlStatus != CURLE_OK) {
        fprintf(
            stderr,
            "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(curlStatus)
        );
        clearRequest(curl, &headers);
        return RC_FAIL_REQUEST;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code < 200 || code > 299) {
        clearRequest(curl, &headers);
        return RC_WRONG_RESPONSE;
    }

    clearRequest(curl, &headers);

    *data = write_result.data;

    return RC_OK;
}

void handleReturnCode(ReturnCode code)
{
    switch (code) {
        case RC_OK:
            break;
        case RC_FAIL_INIT_CURL:
            fprintf(stderr, "FAIL_INIT_CURL\n");
            break;
        case RC_FAIL_BEARER_TOKEN:
            fprintf(stderr, "FAIL_BEARER_TOKEN\n");
            break;
        case RC_FAIL_REQUEST:
            fprintf(stderr, "FAIL_REQUEST\n");
            break;
        case RC_WRONG_RESPONSE:
            fprintf(stderr, "WRONG_RESPONSE\n");
            break;
        default:
            fprintf(stderr, "Untreated error code: %d\n", (int) code);
    }
}
