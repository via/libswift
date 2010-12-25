
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "swift.h"

static size_t
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
    case SWIFT_STATE_NODELIST:
      if (strncmp("X-Account-Container-Count: ", temp, 26) == 0) {
        sscanf(temp, "X-Account-Container-Count: %d", &context->num_containers);
      }
      break;
  }

  free(temp);
  return size * nmemb;
}

static size_t
swift_body_callback(void *ptr, size_t size, size_t nmemb, void *user) {

  struct swift_context *context = (struct swift_context *)user;

  switch (context->state) {
    case SWIFT_STATE_NODELIST:

  return size * nmemb;
}

static swift_error
swift_authenticate(struct swift_context *context) {
  
  struct curl_slist *headerlist = NULL;
  unsigned long response;
  char temp[128];

  context->valid_auth = 0;
  context->state = SWIFT_STATE_AUTH;
  curl_easy_reset(context->curlhandle);

  curl_easy_setopt(context->curlhandle, CURLOPT_HEADERFUNCTION, 
      swift_header_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEHEADER, context);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEFUNCTION,
      swift_body_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, context->connecturl);

  sprintf(temp, "X-Storage-User: %s", context->username);
  headerlist = curl_slist_append(headerlist, temp);
  sprintf(temp, "X-Storage-Pass: %s", context->password);
  headerlist = curl_slist_append(headerlist, temp);

  curl_easy_setopt(context->curlhandle, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_perform(context->curlhandle);
  curl_slist_free_all(headerlist);

  curl_easy_getinfo(context->curlhandle, CURLINFO_RESPONSE_CODE, &response);

  if (response != 204) {
    context->valid_auth = 0;
    return SWIFT_ERROR_PERMISSIONS;
  }

  return SWIFT_SUCCESS;
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

swift_error
swift_node_list(struct swift_context *context, const char *path,
    int *n_entries, char ***contents) {

  swift_error s_err;
  char *url;
  unsigned long response;
  struct curl_slist *headerlist = NULL;

  if (!context->valid_auth) {
    if (s_err = swift_authenticate(context)) {
      return s_err;
    }
  }

  url = (char *)malloc(strlen(path) + strlen(context->authurl) + 1);
  if (!url) {
    return SWIFT_ERROR_MEMORY;
  }
  sprintf(url, "%s%s", context->authurl, path);

  context->state = SWIFT_STATE_NODELIST;
  curl_easy_reset(context->curlhandle);
  curl_easy_setopt(context->curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(context->curlhandle, CURLOPT_HEADERFUNCTION, swift_header_callback);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEHEADER, context);
  curl_easy_setopt(context->curlhandle, CURLOPT_WRITEFUNCTION, swift_body_callback);

  headerlist = curl_slist_append(headerlist, context->authtoken);
  curl_easy_setopt(context->curlhandle, CURLOPT_HTTPHEADER, headerlist);

  curl_easy_perform(context->curlhandle);
  curl_slist_free_all(headerlist);
  curl_easy_getinfo(context->curlhandle, CURLINFO_RESPONSE_CODE, &response);

  if (response != 200) {
    return SWIFT_ERROR_NOTFOUND;
  }


  free(url);
}
