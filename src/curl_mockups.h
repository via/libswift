#ifndef CURL_MOCKUPS_H
#define CURL_MOCKUPS_H

struct test_curl_params {
  char *url;
  char *request;
  int nobody;
  int upload;
  int infilesize;

  void *readdata;
  void *writedata;
  void *headerdata;
  curl_read_callback readfunc;
  curl_write_callback writefunc;
  curl_write_callback headerfunc;
  
  struct curl_slist *headers;

  int response_code;
};

extern struct test_curl_params params;

CURLcode test_curl_easy_perform(CURL *);
void test_curl_easy_reset(CURL *);
CURL *test_curl_easy_init();
void test_curl_easy_cleanup(CURL *);
CURLcode test_curl_easy_getinfo(CURL *, CURLINFO, void *);
CURLcode test_curl_easy_setopt(CURL *, CURLoption, ...);

struct test_curl_params *test_curl_getparams(); 

#endif
