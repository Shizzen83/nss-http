#include <errno.h>
#include <jansson.h>
#include <nss.h>
#include <shadow.h>
#include <string.h>
#include <sys/types.h>
#include "nss_http.h"

static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;


// -1 Failed to parse
// -2 Buffer too small
static int
pack_shadow_struct(json_t *root, struct spwd *result, char *buffer, size_t buflen)
{

    char *next_buf = buffer;
    size_t bufleft = buflen;

    if (!json_is_object(root)) return -1;

    json_t *j_sp_namp = json_object_get(root, "sp_namp");
    json_t *j_sp_pwdp = json_object_get(root, "sp_pwdp");
    json_t *j_sp_lstchg = json_object_get(root, "sp_lstchg");
    json_t *j_sp_min = json_object_get(root, "sp_min");
    json_t *j_sp_max = json_object_get(root, "sp_max");
    json_t *j_sp_warn = json_object_get(root, "sp_warn");
    json_t *j_sp_inact = json_object_get(root, "sp_inact");
    json_t *j_sp_expire = json_object_get(root, "sp_expire");
    json_t *j_sp_flag = json_object_get(root, "sp_flag");

    if (!json_is_string(j_sp_namp)) return -1;
    if (!json_is_string(j_sp_pwdp)) return -1;
    if (!json_is_integer(j_sp_lstchg)) return -1;
    if (!json_is_integer(j_sp_min)) return -1;
    if (!json_is_integer(j_sp_max)) return -1;
    if (!json_is_integer(j_sp_warn)) return -1;
    if (!json_is_integer(j_sp_inact) && !json_is_null(j_sp_inact)) return -1;
    if (!json_is_integer(j_sp_expire) && !json_is_null(j_sp_expire)) return -1;
    if (!json_is_integer(j_sp_flag) && !json_is_null(j_sp_flag)) return -1;

    memset(buffer, '\0', buflen);

    if (bufleft <= j_strlen(j_sp_namp)) return -2;
    result->sp_namp = strncpy(next_buf, json_string_value(j_sp_namp), bufleft);
    next_buf += strlen(result->sp_namp) + 1;
    bufleft  -= strlen(result->sp_namp) + 1;

    if (bufleft <= j_strlen(j_sp_pwdp)) return -2;
    result->sp_pwdp = strncpy(next_buf, json_string_value(j_sp_pwdp), bufleft);
    next_buf += strlen(result->sp_pwdp) + 1;
    bufleft  -= strlen(result->sp_pwdp) + 1;

    // Yay, ints are so easy!
    result->sp_lstchg = json_integer_value(j_sp_lstchg);
    result->sp_min = json_integer_value(j_sp_min);
    result->sp_max = json_integer_value(j_sp_max);
    result->sp_warn = json_integer_value(j_sp_warn);

    if (!json_is_null(j_sp_inact)) result->sp_inact = json_integer_value(j_sp_inact);
    else result->sp_inact = -1;

    if (!json_is_null(j_sp_expire)) result->sp_expire = json_integer_value(j_sp_expire);
    else result->sp_expire = -1;

    if (!json_is_null(j_sp_flag)) result->sp_flag = json_integer_value(j_sp_flag);
    else result->sp_flag = ~0ul;

    return 0;
}


enum nss_status
_nss_http_setspent_locked(int stayopen)
{
    char url[STRING_SIZE];
    json_t *json_root;
    json_error_t json_error;

    snprintf(
        url,
        STRING_SIZE,
        "%s/%s",
        getenv("NSS_HTTP_BACKEND_URL"),
        getenv("NSS_HTTP_FULL_SHADOW_ENDPOINT")
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


// Called to open the shadow file
enum nss_status
_nss_http_setspent(int stayopen)
{
    return NSS_STATUS_SUCCESS;
}


enum nss_status
_nss_http_endspent(void)
{
    if (ent_json_root){
        while (ent_json_root->refcount > 0) json_decref(ent_json_root);
    }
    ent_json_root = NULL;
    ent_json_idx = 0;
    return NSS_STATUS_SUCCESS;
}


enum nss_status
_nss_http_getspent_r(struct spwd *result, char *buffer, size_t buflen, int *errnop)
{
    enum nss_status ret = NSS_STATUS_SUCCESS;

    if (ent_json_root == NULL) {
        ret = _nss_http_setspent_locked(0);
    }

    if (ret != NSS_STATUS_SUCCESS) return ret;

    int pack_result = pack_shadow_struct(
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


enum nss_status
_nss_http_getspnam_r(const char *name, struct spwd *result, char *buffer, size_t buflen, int *errnop)
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
        getenv("NSS_HTTP_NAME_SHADOW_ENDPOINT")
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

    int pack_result = pack_shadow_struct(json_root, result, buffer, buflen);
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
