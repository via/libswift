#include <check.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <stdarg.h>

#include "../src/swift.h"
#include "../src/swift_private.h"

#include "../src/curl_mockups.h"

#ifndef UNITTEST
#error Must configure with --enable-unittest
#endif


int slist_contains(struct curl_slist *list, const char *str) {

  struct curl_slist *cur_entry = list;

  while (cur_entry != NULL) {
    if (strcmp(cur_entry->data, str) != 0) {
      return 1;
    }
    cur_entry = cur_entry->next;
  }
  return 0;
}

START_TEST (test_swift_response) {
  
  fail_unless(swift_response(200) == SWIFT_SUCCESS);
  fail_unless(swift_response(201) == SWIFT_SUCCESS);
  fail_unless(swift_response(204) == SWIFT_SUCCESS);
  fail_unless(swift_response(404) == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_response(401) == SWIFT_ERROR_PERMISSIONS);
  fail_unless(swift_response(400) == SWIFT_ERROR_INTERNAL);

  fail_unless(swift_response(405) == SWIFT_ERROR_UNKNOWN);
  fail_unless(swift_response(205) == SWIFT_ERROR_UNKNOWN);
}
END_TEST

START_TEST (test_swift_chomp) {
  char teststr[64];

  strcpy(teststr, "Test\nTest\n");
  swift_chomp(teststr);
  fail_if(strcmp("Test\nTest", teststr) != 0);

  strcpy(teststr, "Test\r\nTest\r\n");
  swift_chomp(teststr);
  fail_if(strcmp("Test\r\nTest", teststr) != 0);

  strcpy(teststr, "Test\nTest");
  swift_chomp(teststr);
  fail_if(strcmp("Test\nTest", teststr) != 0);

  strcpy(teststr, "\n\n");
  swift_chomp(teststr);
  fail_if(strcmp("\n", teststr) != 0);

  strcpy(teststr, "\n");
  swift_chomp(teststr);
  fail_if(strcmp("", teststr) != 0);

  strcpy(teststr, "");
  swift_chomp(teststr);
  fail_if(strcmp("", teststr) != 0);
}
END_TEST

START_TEST (test_swift_set_headers) {

  struct curl_slist *list;
  CURL *c = curl_easy_init();

  /*Test1 */
  list = swift_set_headers(c, 0);
  fail_unless(list == NULL);
  curl_slist_free_all(list);

  list = swift_set_headers(c, 1, "blah");
  fail_unless(list->next == NULL);
  fail_if(strcmp("blah", list->data) != 0);
  curl_slist_free_all(list);

  list = swift_set_headers(c, 2, "blah1", "blah2");
  fail_if(list->next == NULL);
  fail_unless(list->next->next == NULL);
  fail_if(strcmp("blah1", list->data) != 0);
  fail_if(strcmp("blah2", list->next->data) != 0);
  curl_slist_free_all(list);

  curl_easy_cleanup(c);

}
END_TEST

START_TEST (test_swift_string_to_list) {

  char teststr[128];
  char **list;

  strcpy(teststr, "Test1\nTest2\nTest3\n");
  swift_string_to_list(teststr, 3, &list);
  fail_if(strcmp(list[0], "Test1") != 0);
  fail_if(strcmp(list[1], "Test2") != 0);
  fail_if(strcmp(list[2], "Test3") != 0);
  fail_unless(list[3] == NULL);
  free(list);

  strcpy(teststr, "");
  swift_string_to_list(teststr, 0, &list);
  fail_unless(list[0] == NULL);
  free(list);

  strcpy(teststr, "\n");
  swift_string_to_list(teststr, 1, &list);
  fail_if(strcmp(list[0], "") != 0);
  fail_unless(list[1] == NULL);
  free(list);
}
END_TEST

START_TEST (test_swift_header_callback_authtoken) {

  struct swift_context c;
  c.state = SWIFT_STATE_AUTH;

  c.authtoken = NULL;
  swift_header_callback("Nonsensical data", 1, 16, (void *)&c);
  fail_if(c.authtoken != NULL);

  swift_header_callback("X-Auth-Token: ABCDEFG\n", 1, 22, (void *)&c);
  fail_if(c.authtoken == NULL);
  fail_if( strcmp(c.authtoken, "X-Auth-Token: ABCDEFG") != 0);
  fail_unless(c.valid_auth == 1);

  swift_header_callback("X-Auth-Token:BADDATA\r\n", 1, 22, (void *)&c);
  fail_if(c.authtoken == NULL);
  fail_if( strcmp(c.authtoken, "X-Auth-Token: ABCDEFG") != 0);
  fail_unless(c.valid_auth == 1);

  swift_header_callback("X-Auth-Token: AAAAAAAAAAAAAAAAAAAAAA\r\n", 2, 18, &c);
  fail_if(c.authtoken == NULL);
  fail_if( strcmp(c.authtoken, "X-Auth-Token: AAAAAAAAAAAAAAAAAAAAAA") != 0);
  fail_unless(c.valid_auth == 1);

  free(c.authtoken);
}
END_TEST

START_TEST (test_swift_header_callback_authurl) {

  struct swift_context c;
  c.state = SWIFT_STATE_AUTH;

  c.authurl = NULL;

  swift_header_callback("Nonsensical data", 1, 16, (void *)&c);
  fail_if(c.authurl != NULL);

  swift_header_callback("X-Storage-Url: ABCDEFG\n", 1, 23, (void *)&c);
  fail_if(c.authurl == NULL);
  fail_if( strcmp(c.authurl, "ABCDEFG") != 0);

  swift_header_callback("X-Storage-Url:BADDATA\r\n", 1, 23, (void *)&c);
  fail_if(c.authurl == NULL);
  fail_if( strcmp(c.authurl, "ABCDEFG") != 0);

  swift_header_callback("X-Storage-Url: AAAAAAAAAAAAAAAAAAAAA\r\n", 2, 19, (void *)&c);
  fail_if(c.authurl == NULL);
  fail_if( strcmp(c.authurl, "AAAAAAAAAAAAAAAAAAAAA") != 0);

  free(c.authurl);
}
END_TEST

START_TEST (test_swift_header_callback_counts) {

  struct swift_context c;
  c.state = SWIFT_STATE_CONTAINERLIST;

  swift_header_callback("X-Account-Container-Count: 20", 1, 29, (void *)&c);
  fail_unless(c.num_containers == 20);

  swift_header_callback("X-Account-Container-Count:\n", 1, 27, (void *)&c);
  fail_unless(c.num_containers == 20);

  swift_header_callback("X-Container-Object-Count: 55\r\n", 1, 28, (void *)&c);
  fail_unless(c.num_objects == 55);
  fail_unless(c.num_containers == 20);

  swift_header_callback("X-Container-Object-Count:\r\n", 1, 25, (void *)&c);
  fail_unless(c.num_objects == 55);
  fail_unless(c.num_containers == 20);

  swift_header_callback("Content-Length: 17743\n", 1, 21, (void *)&c);
  fail_unless(c.obj_length == 17743);

  swift_header_callback("Content-Length:\r\n", 1, 17, (void *)&c);
  fail_unless(c.obj_length == 17743);
  fail_unless(c.num_objects == 55);
  fail_unless(c.num_containers == 20);

}
END_TEST

START_TEST (test_swift_body_callback_objlist) {

  struct swift_context c;
  int retval;

  memset(&c, 0, sizeof(c));
  c.state = SWIFT_STATE_OBJECTLIST;
  c.obj_length = 18;

  retval = swift_body_callback("Test1\nTe", 2, 4, (void *)&c);
  fail_unless(retval == 8);

  retval = swift_body_callback("st2", 1, 3, (void *)&c);
  fail_unless(retval == 3);

  retval = swift_body_callback("\nTest3\n", 7, 1, (void *)&c);
  fail_unless(retval == 7);

  retval = swift_body_callback("", 0, 0, (void *)&c);
  fail_unless(retval == 0);

  fail_if(strcmp(c.buffer, "Test1\nTest2\nTest3\n") != 0);

  /*Buffer is already full, this should fail */
  retval = swift_body_callback("Test4\n", 6, 1, (void *)&c);
  fail_unless(retval == 0);

  free(c.buffer);
}
END_TEST

START_TEST (test_swift_body_callback_objread) {

  struct swift_context c;
  char teststr[21];
  int retval;

  memset(&c, 0, sizeof(c));
  memset(teststr, 0, 21);

  c.state = SWIFT_STATE_OBJECT_READ;

  retval = swift_body_callback("Test1", 5, 1, (void *)&c);
  fail_unless(retval == 0);

  c.buffer = teststr;
  retval = swift_body_callback("Test1", 5, 1, (void *)&c);
  fail_unless(retval == 0);

  c.obj_length = 20;
  retval = swift_body_callback("Test1", 5, 1, (void *)&c);
  fail_unless(retval == 5);

  retval = swift_body_callback("", 0, 1, (void *)&c);
  fail_unless(retval == 0);

  retval = swift_body_callback("Test2Test3", 5, 2, (void *)&c);
  fail_unless(retval == 10);

  retval = swift_body_callback("Test4Test5", 2, 5, (void *)&c);
  printf("retval=%d\n", retval);
  fail_unless(retval == 5);

  fail_if(strcmp(c.buffer, "Test1Test2Test3Test4") != 0);

}
END_TEST

START_TEST (test_swift_upload_callback) {

  struct swift_context c;
  char teststr[22];
  char testbuf[21];
  int retval;

  memset(&c, 0, sizeof(c));
  memset(testbuf, 0, 21);
  c.buffer = teststr;
  c.obj_length = 20;
  c.state = SWIFT_STATE_AUTH;

  strcpy(teststr, "Test1");
  retval = swift_upload_callback(testbuf, 1, 5, (void *)&c);
  fail_unless(retval == CURL_READFUNC_ABORT);
  fail_if(strcmp("", testbuf) != 0);

  c.state = SWIFT_STATE_OBJECT_WRITE;

  strcpy(teststr, "Test1Test2Test3Test4A");

  /*Test first word: "Test1" */
  memset(testbuf, 0, 21);
  retval = swift_upload_callback(testbuf, 5, 1, (void *)&c);
  fail_unless(retval == 5);
  fail_if(strcmp("Test1", testbuf) != 0);

  /*Test rest of buffer size, "Test2Test3Test4" */
  memset(testbuf, 0, 21);
  retval = swift_upload_callback(testbuf, 3, 5, (void *)&c);
  fail_unless(retval == 15);
  fail_if(strcmp(testbuf, "Test2Test3Test4") != 0);

  /*Attempt one more character after maximum size*/
  memset(testbuf, 0, 21);
  retval = swift_upload_callback(teststr, 1, 1, (void *)&c);
  fail_unless(retval == 0);
  fail_if(strcmp(testbuf, "") != 0);

}
END_TEST


START_TEST (test_swift_context_create) {

  struct swift_context *c;
  fail_if(swift_context_create(&c, "blah1", "blah2", "blah3"));
  fail_if(strcmp("blah1", c->connecturl) != 0);
  fail_if(strcmp("blah2", c->username) != 0);
  fail_if(strcmp("blah3", c->password) != 0);
  swift_context_delete(&c);

  fail_if(swift_context_create(&c, "", "", ""));
  fail_if(strcmp("", c->connecturl) != 0);
  fail_if(strcmp("", c->username) != 0);
  fail_if(strcmp("", c->password) != 0);
  swift_context_delete(&c);
}
END_TEST


START_TEST (test_swift_node_list_setup) {

  struct swift_context c;
  char *url;


  c.curlhandle = curl_easy_init();
  c.authurl = "http://swiftbox";

  fail_unless(swift_node_list_setup(&c, "") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_node_list_setup(NULL, NULL) == SWIFT_ERROR_NOTFOUND);

  fail_unless(swift_node_list_setup(&c, "/") == SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_CONTAINERLIST);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &url);
  fail_if(strcmp("http://swiftbox/", url) != 0);

  fail_unless(swift_node_list_setup(&c, "/mypath") == SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_OBJECTLIST);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &url);
  fail_if(strcmp("http://swiftbox/mypath", url) != 0);

  curl_easy_cleanup(c.curlhandle);

}
END_TEST

START_TEST (test_swift_node_list_free) {

  char *string;
  char **list;

  string = (char *)malloc(64);
  list = (char **)malloc(sizeof(char *) * 5);
  strcpy(string, "Test1\0Test2\0Test3\0Test4");

  list[0] = &string[0];
  list[1] = &string[6];
  list[2] = &string[12];
  list[3] = &string[18];
  list[4] = NULL;
  swift_node_list_free(&list);

  fail_unless(list == NULL);

}
END_TEST

START_TEST (test_swift_container_create_setup) {

  struct swift_context c;
  char *data;
  struct test_curl_params *params = test_curl_getparams();

  fail_unless(swift_container_create_setup(NULL, "test") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_container_create_setup(&c, NULL) == SWIFT_ERROR_NOTFOUND);

  c.curlhandle = curl_easy_init();
  c.authurl = "http://swiftbox";

  fail_if(swift_container_create_setup(&c, "") != SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_CONTAINER_CREATE);
  fail_if(strcmp(params->request, "PUT") != 0);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &data);
  fail_if(strcmp(data, "http://swiftbox/") != 0);

  fail_if(swift_container_create_setup(&c, "testcont") != SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_CONTAINER_CREATE);
  fail_if(strcmp(params->request, "PUT") != 0);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &data);
  fail_if(strcmp(data, "http://swiftbox/testcont") != 0);

  curl_easy_cleanup(c.curlhandle);

}
END_TEST


START_TEST (test_swift_container_delete_setup) {

  struct swift_context c;
  char *data;
  struct test_curl_params *params = test_curl_getparams();

  fail_unless(swift_container_delete_setup(NULL, "test") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_container_delete_setup(&c, NULL) == SWIFT_ERROR_NOTFOUND);

  c.curlhandle = curl_easy_init();
  c.authurl = "http://swiftbox";

  fail_if(swift_container_delete_setup(&c, "") != SWIFT_ERROR_EXISTS);

  fail_if(swift_container_delete_setup(&c, "testcont") != SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_CONTAINER_DELETE);
  fail_if(strcmp(params->request, "DELETE") != 0);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &data);
  fail_if(strcmp(data, "http://swiftbox/testcont") != 0);

  curl_easy_cleanup(c.curlhandle);

}
END_TEST


START_TEST (test_swift_object_exists_setup) {

  struct swift_context c;
  char *data;
  struct test_curl_params *params = test_curl_getparams();

  fail_unless(swift_object_exists_setup(NULL, "", "") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_object_exists_setup(&c, NULL, "") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_object_exists_setup(&c, "", NULL) == SWIFT_ERROR_NOTFOUND);

  c.curlhandle = curl_easy_init();
  c.authurl = "http://swiftbox";


  fail_if(swift_object_exists_setup(&c, "testcont", "testobj") != SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_OBJECT_EXISTS);
  fail_unless(params->nobody == 1);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &data);
  fail_if(strcmp(data, "http://swiftbox/testcont/testobj") != 0);

  curl_easy_cleanup(c.curlhandle);

}
END_TEST


START_TEST (test_swift_object_delete_setup) {

  struct swift_context c;
  char *data;
  struct test_curl_params *params = test_curl_getparams();

  fail_unless(swift_object_delete_setup(NULL, "", "") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_object_delete_setup(&c, NULL, "") == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_object_delete_setup(&c, "", NULL) == SWIFT_ERROR_NOTFOUND);

  c.curlhandle = curl_easy_init();
  c.authurl = "http://swiftbox";

  fail_if(swift_object_delete_setup(&c, "testcont", "testobj") != SWIFT_SUCCESS);
  fail_unless(c.state == SWIFT_STATE_OBJECT_DELETE);
  fail_if(strcmp(params->request, "DELETE") != 0);
  curl_easy_getinfo(c.curlhandle, CURLINFO_EFFECTIVE_URL, &data);
  fail_if(strcmp(data, "http://swiftbox/testcont/testobj") != 0);

  curl_easy_cleanup(c.curlhandle);
}
END_TEST


START_TEST (test_swift_free_transfer_handle) {

  struct swift_transfer_handle *handle = NULL;

  /* Can't test a return value, but this will at least make sure
   * the function doesn't segfault */
  swift_free_transfer_handle(NULL);
  swift_free_transfer_handle(&handle);

  handle = (struct swift_transfer_handle *)malloc(
      sizeof(struct swift_transfer_handle));

  handle->ptr = malloc(1);
  handle->object = malloc(1);
  handle->container = malloc(1);

  swift_free_transfer_handle(&handle);
  fail_unless(handle == NULL);

}
END_TEST


START_TEST (test_swift_create_transfer_handle) {

  struct swift_transfer_handle *handle = NULL;
  struct swift_context context;

  const char * obj = "testobj";
  const char * cont = "testcont";
  const int n_bytes = 100;

  char * tempobj = NULL;
  char * tempcont = NULL;

  fail_unless(swift_create_transfer_handle(&context, NULL, "", &handle, 0) ==
      SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_create_transfer_handle(NULL, "", "", &handle, 0) ==
      SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_create_transfer_handle(&context, "", NULL, &handle, 0) ==
      SWIFT_ERROR_NOTFOUND);                            
  fail_unless(swift_create_transfer_handle(&context, "", "", NULL, 0) ==
      SWIFT_ERROR_NOTFOUND);                            

  tempobj = (char *)malloc(strlen(obj) + 1);
  tempcont = (char *)malloc(strlen(cont) + 1);
  strcpy(tempobj, obj);
  strcpy(tempcont, cont);

  fail_if(swift_create_transfer_handle(&context, tempcont, tempobj,
        &handle, n_bytes) != SWIFT_SUCCESS);
  free(tempobj);
  free(tempcont); /*Function must make its own copy */


  fail_if(strcmp(handle->container, cont) != 0);
  fail_if(strcmp(handle->object, obj) != 0);
  fail_unless(handle->parent == &context);
  fail_unless(handle->length == n_bytes);
  fail_unless(handle->fpos == 0);

  /* We should be able to set 100 bytes of data, no segfaults */
  memset(handle->ptr, 0, n_bytes);

}
END_TEST


START_TEST (test_swift_sync_setup_read) {

  struct swift_context c;
  struct swift_transfer_handle h;
  struct test_curl_params *params = test_curl_getparams();
  char tempbuf[100];

  memset(&h, 0, sizeof(h));

  fail_unless(swift_sync_setup(NULL) == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_sync_setup(&h) == SWIFT_ERROR_NOTFOUND);

  c.authurl = "http://swiftbox";

  h.parent = &c;
  h.container = "testcont";
  h.object = "testobj";
  h.ptr = tempbuf;
  h.length = 100;
  h.mode = SWIFT_READ;

  fail_unless(swift_sync_setup(&h) == SWIFT_SUCCESS);
  fail_if(strcmp(params->url, "http://swiftbox/testcont/testobj") != 0);
  fail_unless(c.state == SWIFT_STATE_OBJECT_READ);
  fail_unless(c.buffer == h.ptr);
  fail_unless(c.buffer_pos == 0);
  fail_unless(c.obj_length == 100);
  fail_unless(params->upload == 0);

}
END_TEST


START_TEST (test_swift_sync_setup_write) {

  struct swift_context c;
  struct swift_transfer_handle h;
  struct test_curl_params *params = test_curl_getparams();
  char tempbuf[100];

  memset(&h, 0, sizeof(h));

  fail_unless(swift_sync_setup(NULL) == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_sync_setup(&h) == SWIFT_ERROR_NOTFOUND);

  c.authurl = "http://swiftbox";

  h.parent = &c;
  h.container = "testcont";
  h.object = "testobj";
  h.ptr = tempbuf;
  h.length = 100;
  h.mode = SWIFT_WRITE;

  fail_unless(swift_sync_setup(&h) == SWIFT_SUCCESS);
  fail_if(strcmp(params->url, "http://swiftbox/testcont/testobj") != 0);
  fail_unless(c.state == SWIFT_STATE_OBJECT_WRITE);
  fail_unless(c.buffer == h.ptr);
  fail_unless(c.buffer_pos == 0);
  fail_unless(c.obj_length == 100);
  fail_unless(params->upload == 1);
  fail_unless(params->infilesize == 100);
  fail_unless(params->readfunc == (curl_read_callback)swift_upload_callback);
  fail_unless(params->readdata == &c);

}
END_TEST

START_TEST (test_swift_perform) {

  const char *token = "AUTHTOKEN";
  const int response = 200;

  struct swift_context c;
  struct test_curl_params *params = test_curl_getparams();

  test_curl_easy_reset(&c);
  c.authtoken = (char *)malloc(strlen(token) + 1);
  strcpy(c.authtoken, token);
  params->response_code = response;

  fail_unless(swift_perform(&c) == response);
  fail_unless(params->headerfunc == (curl_write_callback)swift_header_callback);
  fail_unless(params->headerdata == &c);
  fail_unless(params->writefunc == (curl_write_callback)swift_body_callback);
  fail_unless(params->writedata == &c);
  
  fail_if(params->headers == NULL);
  fail_if(strcmp(params->headers->data, token) != 0);
  fail_unless(params->headers->next == NULL);

  test_curl_easy_reset(&c);
  free(c.authtoken);

}
END_TEST


START_TEST (test_swift_authenticate) {

  char *user = "testuser";
  char *pass = "testpass";
  const int response = 200;

  struct swift_context c;
  struct test_curl_params *params = test_curl_getparams();

  memset(&c, 0, sizeof(c));
  test_curl_easy_reset(&c);
  params->response_code = response;

  /* Test degenerates */
  fail_unless(swift_authenticate(NULL) == SWIFT_ERROR_NOTFOUND);
  fail_unless(swift_authenticate(&c) == SWIFT_ERROR_NOTFOUND);

  c.username = user;
  c.password = pass;
  c.connecturl = "http://swiftbox:11000";

  fail_unless(swift_authenticate(&c) == SWIFT_SUCCESS);
  fail_unless(c.valid_auth == 0);
  fail_unless(c.state == SWIFT_STATE_AUTH);
  fail_unless(slist_contains(params->headers, "X-Storage-User: testuser"));
  fail_unless(slist_contains(params->headers, "X-Storage-Pass: testpass"));
  fail_unless(params->headerfunc == (curl_write_callback)swift_header_callback);
  fail_unless(params->headerdata == &c);
  fail_unless(params->writefunc == (curl_write_callback)swift_body_callback);
  fail_unless(params->writedata == &c);
  fail_unless(strcmp(params->url, c.connecturl) == 0);

  test_curl_easy_reset(&c);

}
END_TEST


START_TEST (test_swift_read) {
  
  struct swift_transfer_handle h;
  char testbuf[20];
  int nbytes = 20;

  /* Degenerates */
  fail_unless(swift_read(NULL, testbuf, nbytes) == 0);
  fail_unless(swift_read(&h, NULL, nbytes) == 0);

  h.length = 20;
  h.fpos = 0;
  h.ptr = malloc(40);
  memset(h.ptr, 0, 40);
  memset(testbuf, 0, 20);
  memcpy(h.ptr, "ABCDEFGHIJKLMNOPQRST", 20);
  
  fail_unless(swift_read(&h, testbuf, 10) == 10);
  fail_if(memcmp(testbuf, "ABCDEFGHIJ", 10) != 0);
  fail_if(memcmp(testbuf + 10, "\0\0\0\0\0\0\0\0\0\0", 10) != 0);

  fail_unless(swift_read(&h, testbuf, 10) == 10);
  fail_if(memcmp(testbuf, "KLMNOPQRST", 10) != 0);
  fail_if(memcmp(testbuf + 10, "\0\0\0\0\0\0\0\0\0\0", 10) != 0);

  fail_unless(swift_read(&h, testbuf, 1) == 0);
  fail_unless(h.fpos == 20);

  h.fpos = 0;
  fail_unless(swift_read(&h, testbuf, 21) == 20);
  fail_unless(h.fpos == 20);
  fail_unless(swift_read(&h, testbuf, 20) == 0);

  free(h.ptr);

}
END_TEST


START_TEST (test_swift_write) {

  struct swift_transfer_handle h;
  char destbuf[20];
  int nbytes = 20;

  /* Degenerates */
  fail_unless(swift_write(NULL, destbuf, nbytes) == 0);
  fail_unless(swift_write(&h, NULL, nbytes) == 0);

  h.length = 20;
  h.fpos = 0;
  h.ptr = destbuf;
  memset(h.ptr, 0, 20);

  fail_unless(swift_write(&h, "ABCDEFGHIJ", 10) == 10);
  fail_unless(memcmp(destbuf, "ABCDEFGHIJ", 10) == 0);
  fail_unless(memcmp(destbuf + 10, "\0\0\0\0\0\0\0\0\0\0", 10) == 0);
  fail_unless(h.fpos == 10);

  fail_unless(swift_write(&h, "KLMNOPQRST", 10) == 10);
  fail_unless(memcmp(destbuf, "ABCDEFGHIJKLMNOPQRST", 20) == 0);
  fail_unless(h.fpos == 20);


  fail_unless(swift_write(&h, "A", 1) == 0);
  fail_unless(h.fpos == 20);

  h.fpos = 0;
  fail_unless(swift_write(&h, "ABCDEFGHIJKLMNOPQRST123", 23) == 20);
  fail_unless(h.fpos == 20);
  fail_unless(memcmp(destbuf, "ABCDEFGHIJKLMNOPQRST", 20) == 0);

}
END_TEST


START_TEST (test_swift_seek) {

  struct swift_transfer_handle h;
  h.length = 20;
  
  swift_seek(&h, 0);
  fail_unless(h.fpos == 0);

  swift_seek(&h, 1);
  fail_unless(h.fpos == 1);

  swift_seek(&h, 19);
  fail_unless(h.fpos == 19);

  swift_seek(&h, 20);
  fail_unless(h.fpos == 19);

  swift_seek(&h, 21);
  fail_unless(h.fpos == 19);

}
END_TEST


START_TEST (test_swift_get_data) {

  struct swift_transfer_handle h;
  void *ptr;
  char testdata;

  h.ptr = NULL;
  h.length = 10;

  fail_unless(swift_get_data(&h, &ptr) == 10);
  fail_unless(ptr == NULL);

  h.ptr = &testdata;
  fail_unless(swift_get_data(&h, &ptr) == 10);
  fail_unless(ptr == &testdata);

}
END_TEST


Suite *swift_suite(void) {
  Suite *s = suite_create("libswift");
  TCase *tc_core = tcase_create("Internal functions");
  TCase *tc_cb = tcase_create("Callback functions");
  TCase *tc_api = tcase_create("API functions");

  tcase_add_test(tc_core, test_swift_response);
  tcase_add_test(tc_core, test_swift_chomp);
  tcase_add_test(tc_core, test_swift_set_headers);
  tcase_add_test(tc_core, test_swift_string_to_list);

  tcase_add_test(tc_api, test_swift_context_create);
  tcase_add_test(tc_api, test_swift_node_list_setup);
  tcase_add_test(tc_api, test_swift_node_list_free);
  tcase_add_test(tc_api, test_swift_container_create_setup);
  tcase_add_test(tc_api, test_swift_container_delete_setup);
  tcase_add_test(tc_api, test_swift_object_exists_setup);
  tcase_add_test(tc_api, test_swift_object_delete_setup);
  tcase_add_test(tc_api, test_swift_free_transfer_handle);
  tcase_add_test(tc_api, test_swift_create_transfer_handle);
  tcase_add_test(tc_api, test_swift_sync_setup_read);
  tcase_add_test(tc_api, test_swift_sync_setup_write);
  tcase_add_test(tc_api, test_swift_perform);
  tcase_add_test(tc_api, test_swift_authenticate);
  tcase_add_test(tc_api, test_swift_read);
  tcase_add_test(tc_api, test_swift_write);
  tcase_add_test(tc_api, test_swift_seek);
  tcase_add_test(tc_api, test_swift_get_data);

  tcase_add_test(tc_cb, test_swift_header_callback_authtoken);
  tcase_add_test(tc_cb, test_swift_header_callback_authurl);
  tcase_add_test(tc_cb, test_swift_header_callback_counts);
  tcase_add_test(tc_cb, test_swift_body_callback_objlist);
  tcase_add_test(tc_cb, test_swift_body_callback_objread);
  tcase_add_test(tc_cb, test_swift_upload_callback);



  suite_add_tcase(s, tc_core);
  suite_add_tcase(s, tc_cb);
  suite_add_tcase(s, tc_api);

  return s;
}

int main(void) {
  int n_failed;
  Suite *s = swift_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  n_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (n_failed == 0) ? EXIT_SUCCESS: EXIT_FAILURE;
}
