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
} swift_error;

typedef enum {
  SWIFT_READ,
  SWIFT_WRITE,
} swift_transfermode;

struct swift_context {
  char *connecturl;

  /* http header related*/
  char *authurl;
  char *authtoken;
  int num_containers;
  int num_objects;
  unsigned long obj_length;

  char *username;
  char *password;
  int valid_auth;
  CURL *curlhandle;
};


struct swift_transfer_handle {
  char *path;
  void *ptr;
  unsigned long length;
  unsigned long fpos;

  int consistant;
  swift_transfermode mode;
};

#define MIN(x,y) ( (x) > (y) ? (y) : (x))



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
    const char *object, struct swift_transfer_handle *, unsigned long length);
swift_error swift_delete_object(struct swift_context *, const char *container, 
    const char *object);
swift_error swift_object_contents(struct swift_context *, const char *container, 
    const char *object, struct swift_transfer_handle *);

swift_error swift_sync(struct swift_context *, struct swift_transfer_handle *);
swift_error swift_free_transfer_handle(struct swift_transfer_handle **);

#endif
