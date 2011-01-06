
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <stdarg.h>

#include "swift.h"
#include "swift_private.h"

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

 
STATIC int
swift_perform(struct swift_context *context)  {

  int response;
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
  unsigned long response;
  char username[64];
  char password[64];

  context->valid_auth = 0;
  context->state = SWIFT_STATE_AUTH;
  curl_easy_reset(context->curlhandle);

  curl_easy_setopt(context->curlhandle, CURLOPT_HEADERFUNCTION, 
      swift_header_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEHEADER, context);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEFUNCTION,
      swift_body_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, context->connecturl);


  sprintf(username, "X-Storage-User: %s", context->username);
  sprintf(password, "X-Storage-Pass: %s", context->password);

  headerlist = swift_set_headers(context->curlhandle, 2, username, password);

  curl_easy_perform(context->curlhandle);
  curl_slist_free_all(headerlist);

  curl_easy_getinfo(context->curlhandle, CURLINFO_RESPONSE_CODE, &response);

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
swift_create_context(struct swift_context ** context,
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
    swift_delete_context(context);
    return SWIFT_ERROR_MEMORY;
  }

  strcpy((*context)->username, username);
  strcpy((*context)->password, password);
  strcpy((*context)->connecturl, connecturl);

  return SWIFT_SUCCESS;
}

swift_error
swift_delete_context(struct swift_context **context) {

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
  free(*contents);

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
swift_create_container_setup(struct swift_context *context, const char *container) {

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
swift_create_container(struct swift_context * context, const char *container) {

  int response;
  swift_error s_err;
  
  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  swift_create_container_setup(context, container);
  response = swift_perform(context);

  return swift_response(response);
}

STATIC swift_error
swift_delete_container_setup(struct swift_context *context, const char *container) {

  char *url;

  if (!context || !container) {
    return SWIFT_ERROR_NOTFOUND;
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
swift_delete_container(struct swift_context * context, const char *container) {
                                                                               
  int response;
  swift_error s_err;
  
  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  if ( (s_err = swift_delete_container_setup(context, container)) ) {
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
    const char *object, unsigned long *length) {
                                                                                
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
swift_delete_object_setup(struct swift_context *context, const char *container,
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
swift_delete_object(struct swift_context *context, const char *container,
    const char *object) {

  int response;
  swift_error s_err;

  if (!context->valid_auth) {
    if ( (s_err = swift_authenticate(context)) ) {
      return s_err;
    }
  }

  if ( (s_err = swift_delete_object_setup(context, container, object)) ) {
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

}
  

STATIC swift_error
swift_create_transfer_handle(struct swift_context *context, const char *container, 
    const char *object, struct swift_transfer_handle **handle, unsigned long length) {

  struct swift_transfer_handle *l_handle;

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
swift_create_object(struct swift_context *context, const char *container,
    const char *object, struct swift_transfer_handle **handle, unsigned long len) {

  unsigned long length;
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
swift_read_object(struct swift_context *context, const char *container,
    const char *object, struct swift_transfer_handle **handle) {

  unsigned long length;
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

swift_error
swift_sync_setup(struct swift_transfer_handle *handle) {

  struct swift_context *context = handle->parent;
  char *url;

  if (!context || !handle->container || !handle->object) {
    return SWIFT_ERROR_NOTFOUND;
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
  return newbytes;
}

size_t
swift_get_data(struct swift_transfer_handle *handle, void **ptr) {

  *ptr = handle->ptr;
  return handle->length;

}


void
swift_seek(struct swift_transfer_handle *handle, unsigned long pos) {

  handle->fpos = pos;

}
  
