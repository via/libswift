#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "curl_mockups.h"

struct test_curl_params params;

CURLcode test_curl_easy_perform(CURL *handle) {
  return CURLE_OK;
}

void test_curl_easy_reset(CURL *handle) {
  free(params.url);
  params.url = NULL;
  free(params.request);
  params.request = NULL;
  
  params.nobody = 0;
  params.upload = 0;
  params.infilesize = 0;
  params.readdata = NULL;
  params.writedata = NULL;
  params.headerdata = NULL;
  params.readfunc = NULL;
  params.writefunc = NULL;
  params.headerfunc = NULL;

  if (params.headers != NULL) {
    curl_slist_free_all(params.headers);
    params.headers = NULL;
  }

  params.response_code = 0;
  
}



CURL *test_curl_easy_init() {
  CURL *handle = curl_easy_init();
  memset(&params, 0, sizeof(params));
  return handle;
}

void test_curl_easy_cleanup(CURL *handle) {
  memset(&params, 0, sizeof(params));
  curl_easy_cleanup(handle);
}

CURLcode test_curl_easy_getinfo(CURL *curl, CURLINFO info, void *data) {
  switch (info) {
    case CURLINFO_RESPONSE_CODE:
       *((int *)(data)) = params.response_code;
       break;
    case CURLINFO_EFFECTIVE_URL:
       *((char **)data) = params.url;
       break;
    default:
       /* Not implemented */
       break;
  }

  return CURLE_OK;
}

CURLcode test_curl_easy_setopt(CURL *handle, CURLoption option, ...) {

  va_list args;
  va_start(args, option);

  switch(option) {
    case CURLOPT_WRITEFUNCTION:
      params.writefunc = va_arg(args, curl_write_callback);
      break;
    case CURLOPT_WRITEDATA:
      params.writedata = va_arg(args, void *);
      break;
    case CURLOPT_READFUNCTION:
      params.readfunc = va_arg(args, curl_read_callback);
      break;
    case CURLOPT_READDATA:
      params.readdata = va_arg(args, void *);
      break;
    case CURLOPT_HEADERFUNCTION:
      params.headerfunc = va_arg(args, curl_write_callback);
      break;
    case CURLOPT_WRITEHEADER:
      params.headerdata = va_arg(args, void *);
      break;
    case CURLOPT_URL:
      free(params.url);
      char *url = va_arg(args, char *);
      params.url = (char *)malloc(strlen(url) + 1);
      strcpy(params.url, url);
      break;
    case CURLOPT_CUSTOMREQUEST:
      free(params.request);
      char *req = va_arg(args, char *);
      params.request = (char *)malloc(strlen(req) + 1);
      strcpy(params.request, req);
      break;
    case CURLOPT_HTTPHEADER:
      if (params.headers != NULL) {
        curl_slist_free_all(params.headers);
        params.headers = NULL;
      }
      struct curl_slist *node = va_arg(args, struct curl_slist *);
      while (node != NULL) {
        params.headers = curl_slist_append(params.headers, node->data);
        node = node->next;
      }
      break;
    case CURLOPT_NOBODY:
      params.nobody = va_arg(args, int);
      break;
    case CURLOPT_INFILESIZE:
      params.infilesize = va_arg(args, int);
      break;
    case CURLOPT_UPLOAD:
      params.upload = va_arg(args, int);
      break;
    default:
      printf("Unhandled options!\n");
      exit(EXIT_FAILURE);
  }
  va_end(args);
  return CURLE_OK;
}

struct test_curl_params *test_curl_getparams() {
  return &params;
}
