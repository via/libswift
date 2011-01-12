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

  const char *options = "u:p:h:c:o:A:N:f:s:";
  char *action = NULL;
  char cur_option;
  memset(cl_opts, 0, sizeof(struct client_options));

  while ((cur_option = (char)getopt(argc, argv, options)) != -1) {
    if (!optarg) {
      usage();
      return 0;
    }
    switch(cur_option) {
      case 's':
        cl_opts->str_filesize = optarg;
        break;
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

int
validate_options(struct client_options *opts) {

  char *endptr;

  if (!opts->url || !opts->username || !opts->password) {
    usage();
    return 0;
  }

  switch(opts->action) {
    case ACTION_NODELIST:
      if (!opts->path) {
        fprintf(stderr, "Must specify path!\n\n");
        usage();
        return 0;
      }
      break;
    case ACTION_CONT_CREATE:
    case ACTION_CONT_DELETE:
    case ACTION_CONT_EXIST:
      if (!opts->container) {
        fprintf(stderr, "Must specify a container!\n\n");
        usage();
        return 0;
      }
      break;
    case ACTION_OBJ_WRITE:
      if (!opts->str_filesize && !opts->filename) {
        fprintf(stderr, "Must specify a file size when uploading from stdin!\n\n");
        usage();
        return 0;
      }
      /* If filesize is provided, use it */
      if (opts->str_filesize) {
        opts->filesize = strtol(opts->str_filesize, &endptr, 10);
        if ( (*opts->str_filesize == '\0') || (*endptr != '\0')) {
          fprintf(stderr, "Invalid file size, \'%s\'\n\n", opts->str_filesize);
          usage();
          return 0;
        }
      }
    case ACTION_OBJ_READ:
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
      fio = fopen(opts->filename, "r");
      if (!fio) {
        fprintf(stderr, "Unable to open file for input: %s\n", opts->filename);
        return NULL;
      }
      if (!opts->str_filesize) {
        /* Set filesize based on size of input file */
        fseek(fio, 0, SEEK_END);
        opts->filesize = ftell(fio);
        rewind(fio);
      }
      break;
    case ACTION_OBJ_READ:
      fio = fopen(opts->filename, "w");
      if (!fio) {
        fprintf(stderr, "Unable to open file for output: %s\n", opts->filename);
        return NULL;
      }
      break;
    default:
      fprintf(stderr, "Specified filename, but not transferring data!\n");
      return NULL;
  }
  return fio;
}

swift_error
execute_nodelist(struct client_options *opts, struct swift_context *c) {

  char **contents = NULL;
  int n_entries, cur_entry = 0;
  swift_error e;

  e = swift_node_list(c, opts->path, &n_entries, &contents);
  if (e == SWIFT_SUCCESS) {
    while (cur_entry < n_entries) {
      fprintf(opts->datahandle, "%s\n", contents[cur_entry]);
      ++cur_entry;
    }
    swift_node_list_free(&contents);
  }
  return e;

}


swift_error
execute_objread(struct client_options *opts, struct swift_context *c) {

  struct swift_transfer_handle *h;
  swift_error e;
  size_t n_bytes;
  char tempbuf[128];

  e = swift_read_object(c, opts->container, opts->object, &h);
  if (e == SWIFT_SUCCESS) {
    while ( n_bytes = swift_read(h, tempbuf, 128)) {
      fwrite(tempbuf, 1, n_bytes, opts->datahandle);
    }
    swift_free_transfer_handle(&h);
  }
  return e;
}


swift_error
execute_objwrite(struct client_options *opts, struct swift_context *c) {

  struct swift_transfer_handle *h;
  swift_error e;
  size_t n_bytes_read;
  size_t n_bytes_written;
  char tempbuf[128];

  e = swift_create_object(c, opts->container, opts->object, &h, opts->filesize);
  if (e == SWIFT_SUCCESS) {
    while (n_bytes_read = fread(tempbuf, 1, 128, opts->datahandle)) {
      n_bytes_written = swift_write(h, tempbuf, n_bytes_read);
      if (n_bytes_written != n_bytes_read) {
        e = SWIFT_ERROR_UNKNOWN;
        break;
      }
    }
    e = swift_sync(h);
    swift_free_transfer_handle(&h);
  }
  return e;
}


swift_error
execute_action(struct client_options *opts, struct swift_context *c) {

  swift_error e;
  unsigned long len;

  switch(opts->action) {
    case ACTION_NODELIST:
      e = execute_nodelist(opts, c);
      break;
    case ACTION_CONT_CREATE:
      e = swift_create_container(c, opts->container);
      break;
    case ACTION_CONT_DELETE:
      e = swift_delete_container(c, opts->container);
      break;
    case ACTION_CONT_EXIST:
      e = swift_container_exists(c, opts->container);
      if (e == SWIFT_SUCCESS) {
        fprintf(opts->datahandle, "Container exists.\n");
      }
      break;
    case ACTION_OBJ_EXIST:
      e = swift_object_exists(c, opts->container, opts->object, &len);
      if (e == SWIFT_SUCCESS) {
        fprintf(opts->datahandle, "Object exists, length %d bytes\n", len);
      }
      break;
    case ACTION_OBJ_DELETE:
      e = swift_delete_object(c, opts->container, opts->object);
      break;
    case ACTION_OBJ_READ:
      e = execute_objread(opts, c);
      break;
    case ACTION_OBJ_WRITE:
      e = execute_objwrite(opts, c);
      break;
    default:
      fprintf(stderr, "Function not implemented!\n");
      break;
  }
  return e;
}
    

int
main(int argc, char ** argv) {

  struct swift_context *c = NULL;
  struct client_options opts;
  swift_error e;

  if (!read_options(&opts, argc, argv)) {
    exit(EXIT_FAILURE);
  }
  if (!validate_options(&opts)) {
    exit(EXIT_FAILURE);
  }
  if (opts.filename) {
    opts.datahandle = setupio(&opts); 
  } else {
    if (opts.action == ACTION_OBJ_WRITE) {
      opts.datahandle = stdin;
    } else {
      opts.datahandle = stdout;
    }
  }
  if (!opts.datahandle) {
    exit(EXIT_FAILURE);
  }


  swift_init();
  e = swift_create_context(&c, opts.url, opts.username, opts.password);
  if (e) {
    fprintf(stderr, "Error: %s, line %d\n", swift_errormsg(e), __LINE__);
    exit(EXIT_FAILURE);
  }

  e = execute_action(&opts, c);
  if (e) {
    fprintf(stderr, "Error: %s, line %d\n", swift_errormsg(e), __LINE__);
    exit(EXIT_FAILURE);
  }

  swift_delete_context(&c);

  fclose(opts.datahandle);

  return 0;

}
