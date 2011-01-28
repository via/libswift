
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <stdarg.h>

#include "swift.h"
#include "swift_private.h"

const char *
swift_errormsg(swift_error e) {

  switch(e) {
    case SWIFT_SUCCESS:
      return "Operation Successful";
    case SWIFT_ERROR_CONNECT:
      return "Could not connect";
    case SWIFT_ERROR_NOTFOUND:
      return "Could not find item";
    case SWIFT_ERROR_UNKNOWN:
      return "Unknown error";
    case SWIFT_ERROR_PERMISSIONS:
      return "Operation not permitted";
    case SWIFT_ERROR_INTERNAL:
      return "Internal error";
    case SWIFT_ERROR_MEMORY:
      return "Could not allocate memory";
    case SWIFT_ERROR_EXISTS:
      return "Object/Container exists";
    default:
      return "Undefined error";
    }
}

STATIC void
swift_chomp(char *str) {

  if (!str) {
    return;
  }

  if (str[strlen(str) - 1] == '\n') {
    str[strlen(str) - 1] = '\0';
  }

  if (str[strlen(str) - 1] == '\r') {
    str[strlen(str) - 1] = '\0';
  }

}
 
STATIC swift_error
swift_response(int response) {
  swift_error s_err;
  switch (response) {
    case 404:
      s_err = SWIFT_ERROR_NOTFOUND;
      break;
    case 401:
      s_err = SWIFT_ERROR_PERMISSIONS;
      break;
    case 400:
      s_err = SWIFT_ERROR_INTERNAL;
      break;
    case 200:
    case 201: /*Fallthrough */
    case 204: /*Fallthrough */
      s_err = SWIFT_SUCCESS;
      break;
    default:
      s_err = SWIFT_ERROR_UNKNOWN;
      break;
  }

  return s_err;
}                                 


STATIC struct curl_slist *
swift_set_headers(CURL *c, int num_headers, ...) {

  struct curl_slist *headerlist = NULL;
  va_list args;

  va_start(args, num_headers);
  while (num_headers--) {
    char *temp = va_arg(args, char *);
    headerlist = curl_slist_append(headerlist, temp);
  }
  va_end(args);

  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headerlist); 
  return headerlist;
}
   

STATIC void
swift_string_to_list(char *string, int n_entries, char ***_list) {
  int cur_list_pos = 0;
  char *cur_str_pos = string;
  char *newpos;
  char **list;
  *_list = (char **)malloc(sizeof(char *) * (n_entries + 1));
  list = *_list;
  list[n_entries] = NULL;

  while (cur_list_pos < n_entries) {
    list[cur_list_pos] = cur_str_pos;
    newpos = strchr(cur_str_pos, '\n');
    if (!newpos) {
      break;
    }
    *newpos = '\0';
    cur_list_pos++;
    cur_str_pos = newpos + 1;
  } 
}
   

STATIC size_t
swift_header_callback(void *ptr, size_t size, size_t nmemb, void *user) {

  struct swift_context *context = (struct swift_context *)user;
  char *temp = NULL;

  /*In order to make the string handling easier, lets copy over the string and
   * append a nul character */
  temp = (char *)malloc(size * nmemb + 1);
  if (!temp)
    return 0;

  strncpy(temp, ptr, size * nmemb);
  temp[size * nmemb] = '\0';
  swift_chomp(temp);

  switch (context->state) {  
    case SWIFT_STATE_AUTH:

      if (strncmp("X-Auth-Token: ", temp,14) == 0) {
        if (context->authtoken) {
          free(context->authtoken);
        }
        context->authtoken = (char *)malloc(size * nmemb + 1);
        strcpy(context->authtoken, temp);
        context->valid_auth = 1;
      } else if (strncmp("X-Storage-Url: ", temp, 15) == 0) {
        if (context->authurl) {
          free(context->authurl);
        }
        context->authurl = (char *)malloc(size * nmemb );
        strcpy(context->authurl, temp + 15);
      }
      break;
    case SWIFT_STATE_CONTAINERLIST:
    case SWIFT_STATE_OBJECTLIST: /*Fallthrough */
    case SWIFT_STATE_OBJECT_EXISTS: /*Fallthrough */
      if (strncmp("X-Account-Container-Count: ", temp, 26) == 0) {
        sscanf(temp, "X-Account-Container-Count: %d", &context->num_containers);
      }
      if (strncmp("X-Container-Object-Count: ", temp, 24) == 0) {
        sscanf(temp, "X-Container-Object-Count: %d", &context->num_objects);
      }
      if (strncmp("Content-Length: ", temp, 16) == 0) {
        sscanf(temp, "Content-Length: %ld", &context->obj_length);
      }
      break;
    default:
      break;
  }

  free(temp);
  return size * nmemb;
}

STATIC size_t
swift_body_callback(void *ptr, size_t size, size_t nmemb, void *user) {

  struct swift_context *context = (struct swift_context *)user;

  switch (context->state) {
    case SWIFT_STATE_CONTAINERLIST:
    case SWIFT_STATE_OBJECTLIST: /*Fallthrough */
      if (!context->buffer) {
        context->buffer = (char *)malloc(context->obj_length + 1);
        if (!context->buffer) {
          return 0;
        }
        context->buffer_pos = 0;
        context->buffer[context->obj_length] = '\0';
      }
      if (context->buffer_pos + size * nmemb > context->obj_length) {
        return 0;
      }
      memcpy(context->buffer + context->buffer_pos, ptr, size * nmemb);
      context->buffer_pos += size * nmemb;

      break;
    case SWIFT_STATE_OBJECT_READ:
      if (!context->buffer) {
        return 0;
      }
      if (context->buffer_pos + size * nmemb > context->obj_length) {
        return 0;
      }

      memcpy(context->buffer + context->buffer_pos, ptr, size * nmemb);
      context->buffer_pos += size * nmemb;
    default:
      break;

  }

  return size * nmemb;
}

STATIC size_t
swift_upload_callback(void *ptr, size_t size, size_t nmemb, void *user) {

  struct swift_context *context = (struct swift_context *)user;
  int newbytes;

  if (context->state != SWIFT_STATE_OBJECT_WRITE) {
    return CURL_READFUNC_ABORT;
  }

  newbytes = (context->obj_length - context->buffer_pos) < size * nmemb ?
    (context->obj_length - context->buffer_pos) :
    size * nmemb;

  memcpy(ptr, context->buffer + context->buffer_pos, newbytes);
  context->buffer_pos += newbytes;

  return newbytes;
}

  
 
STATIC int
swift_perform(struct swift_context *context)  {

  long response;
  struct curl_slist *headers = NULL;

  headers = swift_set_headers(context->curlhandle, 1, context->authtoken);
  curl_easy_setopt(context->curlhandle, CURLOPT_HEADERFUNCTION, swift_header_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEHEADER, context);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEFUNCTION, swift_body_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEDATA, context);
  curl_easy_perform(context->curlhandle);
  curl_slist_free_all(headers);

  curl_easy_getinfo(context->curlhandle, CURLINFO_RESPONSE_CODE, &response);
  return response;
}                                   

STATIC swift_error
swift_authenticate(struct swift_context *context) {
  

  struct curl_slist *headerlist = NULL;
  long response;
  const char *usertag = "X-Storage-User: ";
  const char *passtag = "X-Storage-Pass: ";
  char *username = NULL;
  char *password = NULL;

  if (!context || !context->username || !context->password) {
    return SWIFT_ERROR_NOTFOUND;
  }

  username = (char *)malloc(strlen(context->username) + 
      strlen(usertag) + 1);
  password = (char *)malloc(strlen(context->password) + 
      strlen(passtag) + 1);

  context->valid_auth = 0;
  context->state = SWIFT_STATE_AUTH;
  curl_easy_reset(context->curlhandle);

  sprintf(username, "%s%s", usertag, context->username);
  sprintf(password, "%s%s", passtag, context->password);

  headerlist = swift_set_headers(context->curlhandle, 2, username, password);

  curl_easy_setopt(context->curlhandle, CURLOPT_URL, context->connecturl);
  curl_easy_setopt(context->curlhandle, CURLOPT_HEADERFUNCTION, swift_header_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEHEADER, context);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEFUNCTION, swift_body_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEDATA, context);

  curl_easy_perform(context->curlhandle);
  curl_slist_free_all(headerlist);

  curl_easy_getinfo(context->curlhandle, CURLINFO_RESPONSE_CODE, &response);

  free(username);
  free(password);

  return swift_response(response);
}


swift_error
swift_init() {
  
  if (curl_global_init(CURL_GLOBAL_ALL)) {
    return SWIFT_ERROR_INTERNAL;
  }

  return SWIFT_SUCCESS;
}


swift_error
swift_deinit() {
  curl_global_cleanup();
  return SWIFT_SUCCESS;
}

swift_error
swift_context_create(struct swift_context ** context,
                     const char *connecturl,
                     const char *username,
                     const char *password) {

  *context = (struct swift_context *)malloc(sizeof(struct swift_context));
  if (!*context) {
    return SWIFT_ERROR_MEMORY;
  }

  memset(*context, 0, sizeof(struct swift_context));

  /*Start with initializing curl*/
  (*context)->curlhandle = curl_easy_init();
  if (!(*context)->curlhandle) {
    return SWIFT_ERROR_INTERNAL;
  }
  
  /* Allocate memory for strings */
  (*context)->username = (char *)malloc(strlen(username) + 1);
  (*context)->password = (char *)malloc(strlen(password) + 1);
  (*context)->connecturl = (char *)malloc(strlen(connecturl) + 1);

  if ( !(*context)->username ||
       !(*context)->password ||
       !(*context)->connecturl ) {
    swift_context_delete(context);
    return SWIFT_ERROR_MEMORY;
  }

  strcpy((*context)->username, username);
  strcpy((*context)->password, password);
  strcpy((*context)->connecturl, connecturl);

  return SWIFT_SUCCESS;
}

swift_error
swift_context_delete(struct swift_context **context) {

  /* Check each allocated object in it and free it*/
  if ((*context)->username) {
    free((*context)->username);
  }

  if ((*context)->password) {
    free((*context)->password);
  }

  if ((*context)->connecturl) {
    free((*context)->connecturl);
  }

  if ((*context)->authurl) {
    free((*context)->authurl);
  }

  if ((*context)->authtoken) {
    free((*context)->authtoken);
  }

  if ((*context)->curlhandle) {
    curl_easy_cleanup((*context)->curlhandle);
  }

  free(*context);


  return SWIFT_SUCCESS;
}

swift_error
swift_can_connect(struct swift_context *context) {

  return swift_authenticate(context);

}

STATIC swift_error
swift_node_list_setup(struct swift_context *context, const char *path) {

  char *url;

  if (!context || !path) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if (strncmp("/", path, 1) != 0) {
    return SWIFT_ERROR_NOTFOUND;
  }

  url = (char *)malloc(strlen(path) + strlen(context->authurl) + 1);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }
  sprintf(url, "%s%s", context->authurl, path);
  
  /* Determine if this is an account listing, or container listing */
  if (strcmp("/", path) == 0) {
    context->state = SWIFT_STATE_CONTAINERLIST;
  } else {
    context->state = SWIFT_STATE_OBJECTLIST;
  }
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);

  free(url);

  return SWIFT_SUCCESS;
}


swift_error
swift_node_list(struct swift_context *context, const char *path,
    int *n_entries, char ***contents) {

  swift_error s_err;
  unsigned long response;

  if (!(context->valid_auth)) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }


  if ( (s_err = swift_node_list_setup(context, path))  ) {
    return s_err;
  }

  response = swift_perform(context);

  if (context->state == SWIFT_STATE_OBJECTLIST) {
    swift_string_to_list(context->buffer, context->num_objects, contents);
    *n_entries = context->num_objects;
  } else {
    swift_string_to_list(context->buffer, context->num_containers, contents);
    *n_entries = context->num_containers;
  }

 return swift_response(response);
}

swift_error
swift_node_list_free(char ***contents) {

  if (!contents)
    return SWIFT_SUCCESS;

  if (!*contents) 
    return SWIFT_SUCCESS;


  free(**contents);
  **contents = NULL;
  free(*contents);
  *contents = NULL;

  return SWIFT_SUCCESS;

}

swift_error
swift_container_exists(struct swift_context *context, const char *container) {

  char **contents = NULL;
  int n_entries;

  char *temp = (char *)malloc(strlen(container) + 2);
  if (!temp) {
    return SWIFT_ERROR_MEMORY;
  }
  strcpy(temp + 1, container);
  temp[0] = '/';

  swift_error e = swift_node_list(context, temp, &n_entries, &contents);
  free(temp);
  if (!e) {
    swift_node_list_free(&contents);
  }
  return e;
}

STATIC swift_error
swift_container_create_setup(struct swift_context *context, const char *container) {

  char *url;

  if (!context || !container) {
    return SWIFT_ERROR_NOTFOUND;
  }
 
  url = (char *)malloc(strlen(container) + strlen(context->authurl) + 2);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }
  sprintf(url, "%s/%s", context->authurl, container); 
  context->state = SWIFT_STATE_CONTAINER_CREATE;
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(context->curlhandle, CURLOPT_CUSTOMREQUEST, "PUT"); 


  return SWIFT_SUCCESS;
}


swift_error
swift_container_create(struct swift_context * context, const char *container) {

  int response;
  swift_error s_err;
  
  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  swift_container_create_setup(context, container);
  response = swift_perform(context);

  return swift_response(response);
}

STATIC swift_error
swift_container_delete_setup(struct swift_context *context, const char *container) {

  char *url;

  if (!context || !container) {
    return SWIFT_ERROR_NOTFOUND;
  }                                                                     

  if (strcmp("", container) == 0) {
    return SWIFT_ERROR_EXISTS;
  }

  url = (char *)malloc(strlen(container) + strlen(context->authurl) + 2);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }
  sprintf(url, "%s/%s", context->authurl, container); 
  context->state = SWIFT_STATE_CONTAINER_DELETE;
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(context->curlhandle, CURLOPT_CUSTOMREQUEST, "DELETE");  

  free(url);

  return SWIFT_SUCCESS;
}


swift_error
swift_container_delete(struct swift_context * context, const char *container) {
                                                                               
  int response;
  swift_error s_err;
  
  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  if ( (s_err = swift_container_delete_setup(context, container)) ) {
    return s_err;
  }

  response = swift_perform(context);

  return swift_response(response);
}

STATIC swift_error
swift_object_exists_setup(struct swift_context *context, const char *container,
    const char *object) {

  char *url;

  if (!context || !container || !object) {
    return SWIFT_ERROR_NOTFOUND;
  }

  url = (char *)malloc(strlen(container) + strlen(context->authurl) +
      strlen(object) + 3);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }
  sprintf(url, "%s/%s/%s", context->authurl, container, object); 
  context->state = SWIFT_STATE_OBJECT_EXISTS;
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(context->curlhandle, CURLOPT_NOBODY, 1);

  free(url);

  return SWIFT_SUCCESS;
}




swift_error
swift_object_exists(struct swift_context *context, const char *container,
    const char *object, size_t *length) {
                                                                                
  int response;
  swift_error s_err;
  
  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  if ( (s_err = swift_object_exists_setup(context, container, object)) ) {
    return s_err;
  }

  response = swift_perform(context);

  *length = context->obj_length;

  return swift_response(response);;
}

STATIC swift_error
swift_object_delete_setup(struct swift_context *context, const char *container,
    const char *object) {

  char *url;

  if (!context || !container || !object) {
    return SWIFT_ERROR_NOTFOUND;
  }

  url = (char *)malloc(strlen(container) + strlen(context->authurl) +
      strlen(object) + 3);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }

  sprintf(url, "%s/%s/%s", context->authurl, container, object); 
  context->state = SWIFT_STATE_OBJECT_DELETE;
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(context->curlhandle, CURLOPT_CUSTOMREQUEST, "DELETE");
  free(url);

  return SWIFT_SUCCESS;
}

swift_error
swift_object_delete(struct swift_context *context, const char *container,
    const char *object) {

  int response;
  swift_error s_err;

  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  if ( (s_err = swift_object_delete_setup(context, container, object)) ) {
    return s_err;
  }

  response = swift_perform(context);

  return swift_response(response);;
}

void
swift_free_transfer_handle(struct swift_transfer_handle **handle) {

  struct swift_transfer_handle *l_handle;

  if (!handle || !*handle)
    return;

  l_handle = *handle;
  free(l_handle->ptr);
  free(l_handle->object);
  free(l_handle->container);
  free(l_handle);
  l_handle = NULL;
  *handle = NULL;
}
  

STATIC swift_error
swift_create_transfer_handle(struct swift_context *context, const char *container, 
    const char *object, struct swift_transfer_handle **handle, size_t length) {

  struct swift_transfer_handle *l_handle;

  if ( !context || !container || !object ||
      !handle) {
    return SWIFT_ERROR_NOTFOUND;
  }

  *handle = (struct swift_transfer_handle *)
    malloc(sizeof(struct swift_transfer_handle));

  if (!*handle) {
    return SWIFT_ERROR_MEMORY;
  }

  /*Set up the various entries in the handle, starting with the path */
  l_handle = *handle;
  l_handle->container = (char *)malloc(strlen(container) + 1);
  if (!l_handle->container) {
    swift_free_transfer_handle(handle);
    return SWIFT_ERROR_MEMORY;
  }

  l_handle->object = (char *)malloc(strlen(object) + 1);
  if (!l_handle->object) {
    swift_free_transfer_handle(handle);
    return SWIFT_ERROR_MEMORY;
  }

  l_handle->ptr = malloc(length);
  if (!l_handle->ptr) {
    swift_free_transfer_handle(handle);
    return SWIFT_ERROR_MEMORY; 
  }

  strcpy(l_handle->container, container);
  strcpy(l_handle->object, object);
  l_handle->parent = context;
  l_handle->length = length;
  l_handle->fpos = 0;

  return SWIFT_SUCCESS;
}


swift_error
swift_object_writehandle(struct swift_context *context, const char *container,
    const char *object, struct swift_transfer_handle **handle, size_t len) {

  size_t length;
  struct swift_transfer_handle *l_handle;
  swift_error s_err;

  if (!context || !container ||
      !object || !handle) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if (swift_object_exists(context, container, object, &length) == SWIFT_SUCCESS) {
    /* If it already exists, refuse to do anything */
    return SWIFT_ERROR_EXISTS;
  }

  if ( (s_err = swift_create_transfer_handle(context, container, object, 
          handle, len))) {
    return s_err;
  }

  /* Make our syntax easier */
  l_handle = *handle;

  memset(l_handle->ptr, 0, len);
  l_handle->mode = SWIFT_WRITE;

  return SWIFT_SUCCESS;
}

swift_error
swift_object_readhandle(struct swift_context *context, const char *container,
    const char *object, struct swift_transfer_handle **handle) {

  size_t length;
  struct swift_transfer_handle *l_handle;
  swift_error s_err;

  if (!context || !container ||
      !object || !handle) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if ( (s_err = swift_object_exists(context, container, object, &length)) ) {
    /* If it already exists, refuse to do anything */
    return s_err;
  }

  if ( (s_err = swift_create_transfer_handle(context, container, object, 
          handle, length))) {
    return s_err;
  }

  l_handle = *handle;
  l_handle->mode = SWIFT_READ;

  return swift_sync(l_handle);
}

STATIC swift_error
swift_sync_setup(struct swift_transfer_handle *handle) {

  struct swift_context *context;
  char *url;
  swift_error s_err;

  if (!handle) {
    return SWIFT_ERROR_NOTFOUND;
  }

  context = handle->parent;

  if (!context || !handle->container || !handle->object) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  url = (char *)malloc(strlen(handle->container) + strlen(context->authurl) +
      strlen(handle->object) + 3);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }

  sprintf(url, "%s/%s/%s", context->authurl, handle->container, handle->object); 
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  context->buffer = handle->ptr;
  context->buffer_pos = 0;
  context->obj_length = handle->length;

  switch (handle->mode) {
    case SWIFT_READ:
      context->state = SWIFT_STATE_OBJECT_READ;
      break;
    case SWIFT_WRITE:
      context->state = SWIFT_STATE_OBJECT_WRITE;
      curl_easy_setopt(context->curlhandle, CURLOPT_UPLOAD, 1);
      curl_easy_setopt(context->curlhandle, CURLOPT_INFILESIZE, context->obj_length);
      curl_easy_setopt(context->curlhandle, CURLOPT_READFUNCTION, 
          swift_upload_callback);
      curl_easy_setopt(context->curlhandle, CURLOPT_READDATA, context);
      break;
  }

  free(url);

  return SWIFT_SUCCESS;
}


swift_error
swift_sync(struct swift_transfer_handle *handle) {

  swift_error s_err;
  int response;

  if (  (s_err = swift_sync_setup(handle) )) {
    return s_err;
  }
  
  response = swift_perform(handle->parent);
  return swift_response(response);
}

size_t
swift_read(struct swift_transfer_handle *handle, void *buf, size_t nbytes) {

  if (!handle || !buf) {
    return 0;
  }

  int newbytes = (handle->length - handle->fpos) < nbytes ?
    (handle->length - handle->fpos) : 
    nbytes;

  memcpy(buf, handle->ptr + handle->fpos, newbytes);
  handle->fpos += newbytes;
  return newbytes;
}


size_t
swift_write(struct swift_transfer_handle *handle, const void *buf, size_t nbytes) {

  if (!handle || !buf) {
    return 0;
  }

  int newbytes = (handle->length - handle->fpos) < nbytes ?
    (handle->length - handle->fpos) : 
    nbytes;

  memcpy(handle->ptr + handle->fpos, buf, newbytes);
  handle->fpos += newbytes;
  return newbytes;

}

size_t
swift_get_data(struct swift_transfer_handle *handle, void **ptr) {

  *ptr = handle->ptr;
  return handle->length;

}


void
swift_seek(struct swift_transfer_handle *handle, unsigned long pos) {

  if (pos < handle->length) {
    handle->fpos = pos;
  }

}
  
swift_error
swift_object_put(struct swift_context *c, char *container,
    char *object, void *data, size_t length) {

  struct swift_transfer_handle handle;

  if (!data || !object || !container || !c) {
    return SWIFT_ERROR_NOTFOUND;
  }

  handle.container = container;
  handle.object = object;
  handle.mode = SWIFT_WRITE;
  handle.parent = c;
  handle.fpos = 0;
  handle.length = length;
  handle.ptr = data;

  return swift_sync(&handle);

}


swift_error
swift_object_get(struct swift_context *c, char *container,
    char *object, void *data, size_t maxlen) {

  struct swift_transfer_handle handle;
  swift_error s_err;
  size_t length;

  if (!data || !object || !container || !c) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if ( (s_err = swift_object_exists(c, container, object, &length)) ) {
    /* If it already exists, refuse to do anything */
    return s_err;
  }

  handle.container = container;
  handle.object = object;
  handle.mode = SWIFT_READ;
  handle.parent = c;
  handle.fpos = 0;
  handle.length = (length > maxlen) ? maxlen : length;
  handle.ptr = data;

  return swift_sync(&handle);

}

STATIC size_t
swift_multi_callback(void *ptr, size_t size, size_t nmemb, void *user) {

  struct swift_multi_op *op = (struct swift_multi_op *)user;

  return op->callback(ptr, size * nmemb, op->userdata);
}

STATIC swift_error
swift_multi_setup(struct swift_multi_op *op) {

  char *url;
  op->curlhandle = curl_easy_init();

  url = (char *)malloc(strlen(op->container) + strlen(op->context->authurl) +
      strlen(op->objname) + 3);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }

  sprintf(url, "%s/%s/%s", op->context->authurl, op->container, op->objname); 

  curl_easy_setopt(op->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(op->curlhandle, CURLOPT_PRIVATE, op);
  if (op->mode == SWIFT_WRITE) {
    curl_easy_setopt(op->curlhandle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(op->curlhandle, CURLOPT_READFUNCTION, swift_multi_callback);
    curl_easy_setopt(op->curlhandle, CURLOPT_READDATA, op);
    curl_easy_setopt(op->curlhandle, CURLOPT_UPLOAD, 1);
    swift_set_headers(op->curlhandle, 2, op->context->authurl, 
        "Transfer-Encoding: chunked");
  } else if (op->mode == SWIFT_READ) {
    curl_easy_setopt(op->curlhandle, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(op->curlhandle, CURLOPT_WRITEFUNCTION, swift_multi_callback);
    curl_easy_setopt(op->curlhandle, CURLOPT_WRITEDATA, op);
    swift_set_headers(op->curlhandle, 1, op->context->authurl);
  }

}

  


swift_error
swift_object_chunked_operation(struct swift_context *context,
    struct swift_multi_op *oplist, unsigned int n_ops) {


  CURLM *multi;
  int cur_entry = 0;
  int n_running = n_ops;
  swift_error s_err;
  struct swift_multi_op *t_op;
  struct CURLMsg *curl_msg;
  int n_msgs;
  long curl_responsecode;


  if (!oplist || !n_ops) {
    return SWIFT_ERROR_NOTFOUND;
  }

  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }
                      
  multi = curl_multi_init();

  while (cur_entry != n_ops) {
    oplist[cur_entry].curlhandle = curl_easy_init();
    swift_multi_setup(&oplist[cur_entry]);
    curl_multi_add_handle(multi, oplist[cur_entry].curlhandle);
    ++cur_entry;
  }

  while (n_ops) {
    while (curl_multi_perform(multi, &n_running) == CURLM_CALL_MULTI_PERFORM);
    /* Block here normally */
    /*Handle return values */
    while ((curl_msg = curl_multi_info_read(multi, &n_msgs)) != NULL) {   
      curl_easy_getinfo(curl_msg->easy_handle, CURLOPT_PRIVATE, &t_op);
      if (curl_msg->msg == CURLMSG_DONE) {
        curl_easy_getinfo(t_op->curlhandle, CURLINFO_RESPONSE_CODE,
            &curl_responsecode);
        t_op->retval = swift_response(curl_responsecode);
        t_op->done = 1;
      }
    }
  }


  return SWIFT_SUCCESS;
}

