#include <check.h>
#include <stdlib.h>
#include <swift.h>
#include <swift_private.h>
#include <curl/curl.h>



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

  swift_header_callback("X-Auth-Token:BADDATA\r\n", 1, 22, (void *)&c);
  fail_if(c.authtoken == NULL);
  fail_if( strcmp(c.authtoken, "X-Auth-Token: ABCDEFG") != 0);

  swift_header_callback("X-Auth-Token: AAAAAAAAAAAAAAAAAAAAAA\r\n", 2, 18, (void *)&c);
  fail_if(c.authtoken == NULL);
  fail_if( strcmp(c.authtoken, "X-Auth-Token: AAAAAAAAAAAAAAAAAAAAAA") != 0);

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


START_TEST (test_swift_create_context) {

  struct swift_context *c;
  fail_if(swift_create_context(&c, "blah1", "blah2", "blah3"));
  fail_if(strcmp("blah1", c->connecturl) != 0);
  fail_if(strcmp("blah2", c->username) != 0);
  fail_if(strcmp("blah3", c->password) != 0);
  swift_delete_context(&c);

  fail_if(swift_create_context(&c, "", "", ""));
  fail_if(strcmp("", c->connecturl) != 0);
  fail_if(strcmp("", c->username) != 0);
  fail_if(strcmp("", c->password) != 0);
  swift_delete_context(&c);
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

  tcase_add_test(tc_api, test_swift_create_context);

  tcase_add_test(tc_cb, test_swift_header_callback_authtoken);
  tcase_add_test(tc_cb, test_swift_header_callback_authurl);



  suite_add_tcase(s, tc_core);
  suite_add_tcase(s, tc_cb);
  suite_add_tcase(s, tc_api);

  return s;
}

int main(void) {
  int n_failed;
  Suite *s = swift_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);
  n_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (n_failed == 0) ? EXIT_SUCCESS: EXIT_FAILURE;
}
