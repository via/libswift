#ifndef SWIFT_PRIVATE_H
#define SWIFT_PRIVATE_H
      
#ifdef UNITTEST
#define STATIC

#undef curl_easy_getinfo
#undef curl_easy_setopt

#define curl_easy_perform(handle) test_curl_easy_perform(handle)
#define curl_easy_init() test_curl_easy_init()
#define curl_easy_cleanup(handle) test_curl_easy_cleanup(handle)
#define curl_easy_getinfo(handle,tag,data) test_curl_easy_getinfo(handle,tag,data)
#define curl_easy_reset(handle) test_curl_easy_reset(handle)
#define curl_easy_setopt(handle,option,param) test_curl_easy_setopt(handle,option,param)
#include "curl_mockups.h"
#else
#define STATIC static
#endif
      


STATIC void swift_chomp(char *);
STATIC swift_error swift_response(int);
STATIC struct curl_slist *swift_set_headers(CURL *, int, ...);
STATIC void swift_string_to_list(char *, int, char ***);

STATIC size_t swift_header_callback(void *, size_t, size_t, void *);
STATIC size_t swift_body_callback(void *, size_t, size_t, void *);
STATIC size_t swift_upload_callback(void *, size_t, size_t, void *);

STATIC swift_error swift_create_transfer_handle(struct swift_context *, const char *,
    const char *, struct swift_transfer_handle **, unsigned long);
STATIC swift_error swift_node_list_setup(struct swift_context *, const char *);
STATIC swift_error swift_container_create_setup(struct swift_context *, const char *);
STATIC swift_error swift_container_delete_setup(struct swift_context *, const char *);
STATIC swift_error swift_object_exists_setup(struct swift_context *, const char *,
    const char *);
STATIC swift_error swift_object_delete_setup(struct swift_context *, const char *,
    const char *);

#endif
