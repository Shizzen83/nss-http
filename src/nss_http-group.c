#include <errno.h>
#include <grp.h>
#include <jansson.h>
#include <nss.h>
#include <string.h>
#include <sys/types.h>
#include "nss_http.h"

static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;


// -1 Failed to parse
// -2 Buffer too small
static int
pack_group_struct(json_t *root, struct group *result, char *buffer, size_t buflen)
{

    char *next_buf = buffer;
    size_t bufleft = buflen;

    if (!json_is_object(root)) return -1;

    json_t *j_member;
    json_t *j_gr_name = json_object_get(root, "gr_name");
    json_t *j_gr_passwd = json_object_get(root, "gr_passwd");
    json_t *j_gr_gid = json_object_get(root, "gr_gid");
    json_t *j_gr_mem = json_object_get(root, "gr_mem");

    if (!json_is_string(j_gr_name)) {
        return -1;
    }
    if (!json_is_string(j_gr_passwd)) {
        return -1;
    }
    if (!json_is_integer(j_gr_gid)) {
        return -1;
    }
    if (!json_is_array(j_gr_mem) && !json_is_string(j_gr_mem)) {
        return -1;
    }

    memset(buffer, '\0', buflen);

    if (bufleft <= j_strlen(j_gr_name)) return -2;
    result->gr_name = strncpy(next_buf, json_string_value(j_gr_name), bufleft);
    next_buf += strlen(result->gr_name) + 1;
    bufleft  -= strlen(result->gr_name) + 1;

    if (bufleft <= j_strlen(j_gr_passwd)) return -2;
    result->gr_passwd = strncpy(next_buf, json_string_value(j_gr_passwd), bufleft);
    next_buf += strlen(result->gr_passwd) + 1;
    bufleft  -= strlen(result->gr_passwd) + 1;

    // Yay, ints are so easy!
    result->gr_gid = json_integer_value(j_gr_gid);

    // Carve off some space for array of members.
    result->gr_mem = (char **)next_buf;
    next_buf += (json_array_size(j_gr_mem) + 1) * sizeof(char *);
    bufleft  -= (json_array_size(j_gr_mem) + 1) * sizeof(char *);

    for(int i = 0; i < json_array_size(j_gr_mem); i++)
    {
        j_member = json_array_get(j_gr_mem, i);
        if (!json_is_string(j_member)) return -1;

        if (bufleft <= j_strlen(j_member)) return -2;
        strncpy(next_buf, json_string_value(j_member), bufleft);
        result->gr_mem[i] = next_buf;

        next_buf += strlen(result->gr_mem[i]) + 1;
        bufleft  -= strlen(result->gr_mem[i]) + 1;

    }

    return 0;
}


enum nss_status
_nss_http_setgrent(int stayopen)
{
    char url[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_FULL_GROUP_ENDPOINT")
    );

    char *response = NULL;
    nss_http_request(url, &response);
    if (!response) {
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        return NSS_STATUS_UNAVAIL;
    }

    if (!json_is_array(json_root)) {
        json_decref(json_root);
        return NSS_STATUS_UNAVAIL;
    }

    ent_json_root = json_root;
    ent_json_idx = 0;

    return NSS_STATUS_SUCCESS;
}


enum nss_status
_nss_http_endgrent(void)
{
    if (ent_json_root){
        while (ent_json_root->refcount > 0) json_decref(ent_json_root);
    }
    ent_json_root = NULL;
    ent_json_idx = 0;
    return NSS_STATUS_SUCCESS;
}


enum nss_status
_nss_http_setgrent_locked(int stayopen)
{
    char url[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_FULL_GROUP_ENDPOINT")
    );

    char *response = NULL;
    nss_http_request(url, &response);
    if (!response) {
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        return NSS_STATUS_UNAVAIL;
    }

    if (!json_is_array(json_root)) {
        json_decref(json_root);
        return NSS_STATUS_UNAVAIL;
    }

    ent_json_root = json_root;
    ent_json_idx = 0;

    return NSS_STATUS_SUCCESS;
}


enum nss_status
_nss_http_getgrent_r(struct group *result, char *buffer, size_t buflen, int *errnop)
{
    enum nss_status ret = NSS_STATUS_SUCCESS;

    if (ent_json_root == NULL) {
        ret = _nss_http_setgrent_locked(0);
    }

    if (ret != NSS_STATUS_SUCCESS) return ret;

    int pack_result = pack_group_struct(
        json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen
    );

    if (pack_result == -1) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    if (pack_result == -2) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    // Return notfound when there's nothing else to read.
    if (ent_json_idx >= json_array_size(ent_json_root)) {
        *errnop = ENOENT;
        return NSS_STATUS_NOTFOUND;
    }

    ent_json_idx++;
    return NSS_STATUS_SUCCESS;
}


// Find a group by gid
enum nss_status
_nss_http_getgrgid_r(gid_t gid, struct group *result, char *buffer, size_t buflen, int *errnop)
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
        getenv("NSS_HTTP_ID_GROUP_ENDPOINT")
    );

    snprintf(url, STRING_SIZE, url_template, gid);

    char *response = NULL;
    nss_http_request(url, &response);
    if (!response) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_group_struct(json_root, result, buffer, buflen);

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


enum nss_status
_nss_http_getgrnam_r(const char *name, struct group *result, char *buffer, size_t buflen, int *errnop)
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
        getenv("NSS_HTTP_NAME_GROUP_ENDPOINT")
    );

    snprintf(url, STRING_SIZE, url_template, name);

    char *response = NULL;
    nss_http_request(url, &response);
    if (!response) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_group_struct(json_root, result, buffer, buflen);

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
