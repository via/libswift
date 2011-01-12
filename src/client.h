#ifndef SWIFT_CLIENT_H
#define SWIFT_CLIENT_H

typedef enum {
  ACTION_NODELIST,
  ACTION_CONT_CREATE,
  ACTION_CONT_DELETE,
  ACTION_CONT_EXIST,
  ACTION_OBJ_READ,
  ACTION_OBJ_WRITE,
  ACTION_OBJ_EXIST,
  ACTION_OBJ_DELETE,
} client_action;

struct client_options {
  char *url;
  char *username;
  char *password;

  char *container;
  char *object;
  char *path;

  char *filename;

  client_action action;
};




/*Prototypes*/
int read_options(struct client_options *, int, char **);
int validate_options(struct client_options *);
FILE *setupio(struct client_options *);

#endif
