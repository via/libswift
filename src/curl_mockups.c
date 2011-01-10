#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

CURLcode test_curl_easy_setopt(CURL *handle, CURLoption option, void *data) {

  switch(option) {
    case CURLOPT_WRITEFUNCTION:
      params.writefunc = data;
      break;
    case CURLOPT_WRITEDATA:
      params.writedata = data;
      break;
    case CURLOPT_READFUNCTION:
      params.readfunc = data;
      break;
    case CURLOPT_READDATA:
      params.readdata = data;
      break;
    case CURLOPT_HEADERFUNCTION:
      params.headerfunc = data;
      break;
    case CURLOPT_WRITEHEADER:
      params.headerdata = data;
      break;
    case CURLOPT_URL:
      free(params.url);
      params.url = (char *)malloc(strlen((char *)data) + 1);
      strcpy(params.url, (char *)data);
      break;
    case CURLOPT_CUSTOMREQUEST:
      free(params.request);
      params.request = (char *)malloc(strlen((char *)data) + 1);
      strcpy(params.request, (char *)data);
      break;
    case CURLOPT_HTTPHEADER:
      if (params.headers != NULL) {
        curl_slist_free_all(params.headers);
        params.headers = NULL;
      }
      struct curl_slist *node = (struct curl_slist *)data;
      while (node != NULL) {
        params.headers = curl_slist_append(params.headers, node->data);
        node = node->next;
      }
      break;
    case CURLOPT_NOBODY:
      params.nobody = ((int)data);
      break;
    case CURLOPT_INFILESIZE:
      params.infilesize = ((int)data);
      break;
    case CURLOPT_UPLOAD:
      params.upload = ((int)data);
      break;
    default:
      printf("Unhandled options!\n");
      exit(EXIT_FAILURE);
  }
  return CURLE_OK;
}

struct test_curl_params *test_curl_getparams() {
  return &params;
}
