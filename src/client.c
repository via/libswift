#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "swift.h"

#include "client.h"

extern char *optarg;

void
usage() {
  fprintf(stderr, 
      "USAGE: swiftclient -u username -p password -h hostname\n"
      "                   [-c container] [-o object] [-A action]\n"
      "                   [-N path]\n"
      "\n"
      "Action can be any of:\n"
      "   createcont -- create a container, named with -c container\n"
      "   deletecont -- delete a container, named with -c container\n"
      "   contexist  -- determines if a container exists, named with -c container\n"
      "   writeobj   -- write to an object, specified with -c and -o, either from\n"
      "                 stdin or from file specified by -f\n"
      "   readobj    -- read an object, specified with -c and -o, either to stdout\n"
      "                 or to a file specified by -f\n"
      "   objexist   -- determines if an object exists, named with -c and -o\n"
      "   deleteobj  -- deletes an object specified with -c and -o\n"
      "\n"
      "   -N path -- list all objects or containers at given path, eg:\n"
      "      -N /mycontainer\n"
      "      -N /\n"
      );
}

client_action
parse_action(const char *action) {

  client_action retval;

  if (!action) {
    usage();
    exit(EXIT_FAILURE);
  }

  if (strcmp("createcont", action) == 0) {
    retval = ACTION_CONT_CREATE;
  } else if (strcmp("deletecont", action) == 0) {
    retval = ACTION_CONT_DELETE;
  } else if (strcmp("contexist", action) == 0) {
    retval = ACTION_CONT_EXIST;
  } else if (strcmp("writeobj", action) == 0) {
    retval = ACTION_OBJ_WRITE;
  } else if (strcmp("readobj", action) == 0) {
    retval = ACTION_OBJ_READ;
  } else if (strcmp("objexist", action) == 0) {
    retval = ACTION_OBJ_EXIST;
  } else if (strcmp("deleteobj", action) == 0) {
    retval = ACTION_OBJ_DELETE;
  } else {
    usage();
    exit(EXIT_FAILURE);
  }
  return retval;
}


int
read_options(struct client_options *cl_opts, int argc, char **argv) {

  const char *options = "u:p:h:c:o:A:N:f:";
  char *action;
  char cur_option;
  memset(cl_opts, 0, sizeof(struct client_options));

  while ((cur_option = (char)getopt(argc, argv, options)) != -1) {
    if (!optarg) {
      usage();
      return 0;
    }
    switch(cur_option) {
      case 'u':
        cl_opts->username = optarg; 
        break;   
      case 'p':
        cl_opts->password = optarg;
        break;
      case 'h':
        cl_opts->url = optarg;
        break;
      case 'c':
        cl_opts->container = optarg;
        break;
      case 'o':
        cl_opts->object = optarg;
        break;
      case 'A':
        action = optarg;
        break;
      case 'N':
        cl_opts->path = optarg;
        break;
      case 'f':
        cl_opts->filename = optarg;
        break;
      case '?':
      default:
        usage();
        return 0;
        break;
    }
    if ((action == NULL) && (cl_opts->path == NULL)) {
      usage();
      return 0;
    }
    if (action != NULL) {
      cl_opts->action = parse_action(action);
    } else {
      cl_opts->action = ACTION_NODELIST;
    }
  }
}

int
validate_options(struct client_options *opts) {

  if (!opts->url || !opts->username || !opts->password) {
    usage();
    return 0;
  }

  switch(opts->action) {
    case ACTION_CONT_CREATE:
    case ACTION_CONT_DELETE:
    case ACTION_CONT_EXIST:
      if (!opts->container) {
        fprintf(stderr, "Must specify a container!\n\n");
        usage();
        return 0;
      }
      break;
    case ACTION_OBJ_READ:
    case ACTION_OBJ_WRITE:
    case ACTION_OBJ_EXIST:
    case ACTION_OBJ_DELETE:
      if (!opts->container || !opts->object) {
        fprintf(stderr, "Must specify a container and object!\n\n");
        usage();
        return 0;
      }
      break;
    default:
      break;
  }
  return 1;
}

FILE *
setupio(struct client_options *opts) {

  FILE *fio;

  switch(opts->action) {
    case ACTION_OBJ_WRITE:
      fio = fopen(opts->filename, "w");
      if (!fio) {
        fprintf(stderr, "Unable to open file for output: %s\n", opts->filename);
        return NULL;
      }
      break;
    case ACTION_OBJ_READ:
      fio = fopen(opts->filename, "r");
      if (!fio) {
        fprintf(stderr, "Unable to open file for input: %s\n", opts->filename);
        return NULL;
      }
      break;
    default:
      fprintf(stderr, "Specified filename, but not transferring data!\n");
      return NULL;
  }
  return fio;
}

    

int
main(int argc, char ** argv) {

  struct swift_context *c = NULL;
  struct client_options opts;
  swift_error e;
  FILE *datahandle;

  if (!read_options(&opts, argc, argv)) {
    exit(EXIT_FAILURE);
  }
  if (!validate_options(&opts)) {
    exit(EXIT_FAILURE);
  }
  if (opts.filename) {
    datahandle = setupio(&opts); 
  } else {
    if (opts.action == ACTION_OBJ_WRITE) {
      datahandle = stdin;
    } else {
      datahandle = stdout;
    }
  }
  if (!datahandle) {
    exit(EXIT_FAILURE);
  }


  swift_init();
  e = swift_create_context(&c, opts.url, opts.username, opts.password);


  swift_delete_context(&c);

  return 0;

}
