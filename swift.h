#ifndef MAIN_H
#define MAIN_H

#include <curl/curl.h>

typedef enum {
  SWIFT_SUCCESS = 0,
  SWIFT_ERROR_CONNECT,
  SWIFT_ERROR_NOTFOUND,
  SWIFT_ERROR_UNKNOWN,
  SWIFT_ERROR_PERMISSIONS,
  SWIFT_ERROR_INTERNAL,
  SWIFT_ERROR_MEMORY,
  SWIFT_ERROR_EXISTS,
} swift_error;

typedef enum {
  SWIFT_READ,
  SWIFT_WRITE,
} swift_transfermode;

typedef enum {
  SWIFT_STATE_AUTH,
  SWIFT_STATE_CONTAINERLIST,
  SWIFT_STATE_OBJECTLIST,
  SWIFT_STATE_CONTAINER_CREATE,
  SWIFT_STATE_CONTAINER_DELETE,
  SWIFT_STATE_OBJECT_EXISTS,
  SWIFT_STATE_OBJECT_DELETE,
  SWIFT_STATE_OBJECT_READ,
  SWIFT_STATE_OBJECT_WRITE,
} swift_state;

struct swift_context {
  char *connecturl;
  swift_state state;

  /* http header related*/
  char *authurl;
  char *authtoken;
  int num_containers;
  int num_objects;
  unsigned long obj_length;

  /* Nodelist stuff */
  char *buffer;
  int buffer_pos;

  char *username;
  char *password;
  int valid_auth;
  CURL *curlhandle;
};


struct swift_transfer_handle {
  char *container;
  char *object;
  void *ptr;
  unsigned long length;
  unsigned long fpos;

  int consistant;
  swift_transfermode mode;
  struct swift_context *parent;
};

swift_error swift_init();
swift_error swift_deinit();

swift_error swift_create_context(struct swift_context **, 
    const char *connecturl, const char *username, const char *password);
swift_error swift_delete_context(struct swift_context **);

swift_error swift_can_connect(struct swift_context *);

swift_error swift_node_list(struct swift_context *, const char *path, 
    int *n_entries, char *** contents);
swift_error swift_node_list_free(char ***contents);

swift_error swift_container_exists(struct swift_context *, const char *container);
swift_error swift_create_container(struct swift_context *, const char *container);
swift_error swift_delete_container(struct swift_context *, const char *container);

swift_error swift_object_exists(struct swift_context *, const char *container, 
    const char *object, unsigned long *length);
swift_error swift_create_object(struct swift_context *, const char *container, 
    const char *object, struct swift_transfer_handle **, unsigned long length);
swift_error swift_delete_object(struct swift_context *, const char *container, 
    const char *object);
swift_error swift_read_object(struct swift_context *, const char *container, 
    const char *object, struct swift_transfer_handle **);

swift_error swift_sync(struct swift_transfer_handle *);
void swift_free_transfer_handle(struct swift_transfer_handle **);

size_t swift_read(struct swift_transfer_handle *, void *buf, size_t nbytes);
size_t swift_write(struct swift_transfer_handle *, const void *buf, size_t n);
size_t swift_get_data(struct swift_transfer_handle *, void **ptr);
void swift_seek(struct swift_transfer_handle *, unsigned long);

#endif
