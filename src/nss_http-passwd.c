#include <errno.h>
#include <jansson.h>
#include <nss.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include "nss_http.h"

static json_t *ent_json_root = NULL;
static unsigned int ent_json_idx = 0;

// -1 Failed to parse
// -2 Buffer too small
static int pack_passwd_struct(
    json_t *root,
    struct passwd *result,
    char *buffer,
    size_t buflen
)
{

    char *next_buf = buffer;
    size_t bufleft = buflen;

    if (!json_is_object(root)) return -1;

    json_t *j_pw_name = json_object_get(root, "pw_name");
    json_t *j_pw_passwd = json_object_get(root, "pw_passwd");
    json_t *j_pw_uid = json_object_get(root, "pw_uid");
    json_t *j_pw_gid = json_object_get(root, "pw_gid");
    json_t *j_pw_gecos = json_object_get(root, "pw_gecos");
    json_t *j_pw_dir = json_object_get(root, "pw_dir");
    json_t *j_pw_shell = json_object_get(root, "pw_shell");

    if (!json_is_string(j_pw_name)) {
        return -1;
    }
    if (!json_is_string(j_pw_passwd)) {
        return -1;
    }
    if (!json_is_integer(j_pw_uid)) {
        return -1;
    }
    if (!json_is_integer(j_pw_gid)) {
        return -1;
    }
    if (!json_is_string(j_pw_gecos) && !json_is_null(j_pw_gecos)) {
        return -1;
    }
    if (!json_is_string(j_pw_dir)) {
        return -1;
    }
    if (!json_is_string(j_pw_shell)) {
        return -1;
    }

    memset(buffer, '\0', buflen);

    if (bufleft <= j_strlen(j_pw_name)) return -2;
    result->pw_name = strncpy(next_buf, json_string_value(j_pw_name), bufleft);
    next_buf += strlen(result->pw_name) + 1;
    bufleft  -= strlen(result->pw_name) + 1;

    if (bufleft <= j_strlen(j_pw_passwd)) return -2;
    result->pw_passwd = strncpy(next_buf, json_string_value(j_pw_passwd), bufleft);
    next_buf += strlen(result->pw_passwd) + 1;
    bufleft  -= strlen(result->pw_passwd) + 1;

    // Yay, ints are so easy!
    result->pw_uid = json_integer_value(j_pw_uid);
    result->pw_gid = json_integer_value(j_pw_gid);

    if (json_is_null(j_pw_gecos))
    {
        if (bufleft <= 1) return -2;
        result->pw_gecos = strncpy(next_buf, "", 1);
        next_buf += 1;
        bufleft -= 1;
    } else {
        if (bufleft <= j_strlen(j_pw_gecos)) return -2;
        result->pw_gecos = strncpy(next_buf, json_string_value(j_pw_gecos), bufleft);
        next_buf += strlen(result->pw_gecos) + 1;
        bufleft  -= strlen(result->pw_gecos) + 1;
    }

    if (bufleft <= j_strlen(j_pw_dir)) return -2;
    result->pw_dir = strncpy(next_buf, json_string_value(j_pw_dir), bufleft);
    next_buf += strlen(result->pw_dir) + 1;
    bufleft  -= strlen(result->pw_dir) + 1;

    if (bufleft <= j_strlen(j_pw_shell)) return -2;
    result->pw_shell = strncpy(next_buf, json_string_value(j_pw_shell), bufleft);
    next_buf += strlen(result->pw_shell) + 1;
    bufleft  -= strlen(result->pw_shell) + 1;

    return 0;
}

enum nss_status _nss_http_setpwent_locked(void)
{
    printf("_nss_http_getpwent_r -1\n");
    char url[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_FULL_PASSWD_ENDPOINT")
    );

    char *response = NULL;
    nss_http_request(url, &response);
    if (!response) {
        return NSS_STATUS_UNAVAIL;
    }
    printf("_nss_http_getpwent_r -2\n");
    json_root = json_loads(response, 0, &json_error);
    //json_sprintf(response);
    printf("_nss_http_getpwent_r 3 => %s\n", response);
    if (!json_root) {
        return NSS_STATUS_UNAVAIL;
    }
    printf("_nss_http_getpwent_r -4\n");
    if (!json_is_array(json_root)) {
        json_decref(json_root);
        return NSS_STATUS_UNAVAIL;
    }

    ent_json_root = json_root;
    ent_json_idx = 0;

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_http_setpwent(void)
{
    printf("_nss_http_setpwent\n");
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_http_endpwent(void)
{
    json_decref(ent_json_root);

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_http_getpwent_r(
    struct passwd *result,
    char *buffer,
    size_t buflen,
    int *errnop
)
{
    printf("_nss_http_getpwent_r\n");
    //return NSS_STATUS_NOTFOUND;
    enum nss_status nssStatus = NSS_STATUS_TRYAGAIN;

    if (ent_json_root == NULL) {
        nssStatus = _nss_http_setpwent_locked();
    }

    printf("_nss_http_getpwent_r 0\n");

    if (nssStatus != NSS_STATUS_SUCCESS) {
        return nssStatus;
    }

    int pack_result = pack_passwd_struct(
        json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen
    );
    printf("_nss_http_getpwent_r 1\n");
    if (pack_result == -1) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }
    printf("_nss_http_getpwent_r 2\n");
    if (pack_result == -2) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    printf("_nss_http_getpwent_r 3\n");

    // Return notfound when there's nothing else to read.
    if (ent_json_idx >= json_array_size(ent_json_root)) {
        *errnop = ENOENT;
        return NSS_STATUS_NOTFOUND;
    }

    printf("_nss_http_getpwent_r 4\n");

    ent_json_idx++;
    return NSS_STATUS_SUCCESS;
}

// Find a passwd by uid
enum nss_status _nss_http_getpwuid_r(
    uid_t uid,
    struct passwd *result,
    char *buffer,
    size_t buflen,
    int *errnop
)
{
    char url[STRING_SIZE];
    char url_template[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url_template,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_ID_PASSWD_ENDPOINT")
    );

    snprintf(url, STRING_SIZE, url_template, uid);

    char *response = NULL;
    ReturnCode rCode = nss_http_request(url, &response);
    if (rCode != RC_OK) {
        *errnop = (int)rCode;
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_passwd_struct(json_root, result, buffer, buflen);

    if (pack_result == -1) {
        json_decref(json_root);
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    if (pack_result == -2) {
        json_decref(json_root);
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    json_decref(json_root);

    return NSS_STATUS_SUCCESS;
}

//Find a passwd by name
enum nss_status _nss_http_getpwnam_r(
    const char *nam,
    struct passwd *result,
    char *buffer,
    size_t buflen,
    int *errnop
)
{
    char url[STRING_SIZE];
    char url_template[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url_template,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_NAME_PASSWD_ENDPOINT")
    );

    snprintf(url, STRING_SIZE, url_template, nam);

    char *response = NULL;
    ReturnCode rCode = nss_http_request(url, &response);

    if (response != NULL) {
        printf("OUTCH\n");
        fprintf(stderr, "RESPONSE: %s", response);
    }
    fprintf(stderr, "tArg");
    if (rCode != RC_OK) {
        *errnop = ENOENT;
        handleReturnCode(rCode);
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_passwd_struct(json_root, result, buffer, buflen);

    if (pack_result == -1) {
        json_decref(json_root);
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    if (pack_result == -2) {
        json_decref(json_root);
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    json_decref(json_root);

    return NSS_STATUS_SUCCESS;
}
