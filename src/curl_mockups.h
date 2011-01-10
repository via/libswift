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
  size_t (*readfunc)(void *, size_t, size_t, void *);
  size_t (*writefunc)(void *, size_t, size_t, void *);
  size_t (*headerfunc)(void *, size_t, size_t, void *);
  
  struct curl_slist *headers;

  int response_code;
};

extern struct test_curl_params params;

CURLcode test_curl_easy_perform(CURL *);
void test_curl_easy_reset(CURL *);
CURL *test_curl_easy_init();
void test_curl_easy_cleanup(CURL *);
CURLcode test_curl_easy_getinfo(CURL *, CURLINFO, void *);
CURLcode test_curl_easy_setopt(CURL *, CURLoption, void *);

struct test_curl_params *test_curl_getparams(); 

#endif
