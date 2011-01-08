#ifndef SWIFT_PRIVATE_H
#define SWIFT_PRIVATE_H
      
#ifdef UNITTEST
#define STATIC
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
STATIC swift_error swift_create_container_setup(struct swift_context *, const char *);
STATIC swift_error swift_delete_container_setup(struct swift_context *, const char *);

#endif
