#include <stdio.h>
#include <stdlib.h>
#include "curl/curl.h"
#include <check.h>

#include "../src/swift.h"

#ifndef INTEGRATION
#error You must specify --enable-integration=user,password,url
#endif

static char *username;
static char *password;
static char *url;

int
parse_info(char *info, char **username, char **password, char **url) {

  *username = strtok(info, ",");
  *password = strtok(NULL, ",");
  *url = strtok(NULL, ",");


  if (!*username || !*password || !*url) {
    return 0;
  }

  return 1;

}


START_TEST (test_reset) {

  struct swift_context *c;
  swift_error e;
  char contname[20];
  char objname[20];
  int container_num;
  int object_num;

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);

  /*In case we failed after creating the 100 objects, try to delete them */
  for (object_num = 0; object_num < 50; ++object_num) {
    sprintf(objname, "testobj%02d", object_num);
    swift_delete_object(c, "testcont00", objname);
    swift_delete_object(c, "testcont01", objname);
  }

  /*Check to see if any of the test containers exist, and if so, delete it */
  for (container_num = 0; container_num < 100; ++container_num) {
    sprintf(contname, "testcont%02d", container_num);
    if(swift_container_exists(c, contname) == SWIFT_SUCCESS) {
      fail_unless(swift_delete_container(c, contname) == SWIFT_SUCCESS);
    }
  }
  swift_delete_context(&c);
      


}
END_TEST



START_TEST (test_container_create) {

  struct swift_context *c;
  swift_error e;
  int container_num;
  char contname[20];

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);
  
  /* Create 100 containers */
  for (container_num = 0; container_num < 100; ++container_num) {
    sprintf(contname, "testcont%02d", container_num);
    e = swift_create_container(c, contname);
    fail_unless(e == SWIFT_SUCCESS);
  } 

  swift_delete_context(&c);

}
END_TEST


START_TEST (test_container_overwrite) {

  struct swift_context *c;
  swift_error e;
  int container_num;
  char contname[20];

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);

  fail_unless(swift_container_exists(c, "testcont00") == SWIFT_SUCCESS);

  /*Make sure we can't overwrite a container.  Need to fix error response code
   * to return correct error values/
   */
  fail_unless(swift_create_container(c, "testcont00") != SWIFT_SUCCESS);

  swift_delete_context(&c);
}
END_TEST


START_TEST (test_objects_create) {
  
  struct swift_context *c;
  struct swift_transfer_handle *h;
  swift_error e;
  int object_num;
  char objname[20];
  void *data;

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);
  /* We will create 50 objects in two containers */
  /* Objects will by 1024 bytes, patterned */

  for (object_num = 0; object_num < 50; ++object_num) {
    sprintf(objname, "testobj%02d", object_num);
    fail_unless(swift_create_object(c, "testcont00", objname, &h, 1024)
        == SWIFT_SUCCESS); 
    fail_unless(swift_get_data(h, &data) == 1024);
    memset(data, 0xAA, 1024);
    fail_unless(swift_sync(h) == SWIFT_SUCCESS);
    swift_free_transfer_handle(&h);
    fail_unless(swift_create_object(c, "testcont01", objname, &h, 1024)
        == SWIFT_SUCCESS); 
    fail_unless(swift_get_data(h, &data) == 1024);
    memset(data, 0xAA, 1024);
    fail_unless(swift_sync(h) == SWIFT_SUCCESS);
    swift_free_transfer_handle(&h);
  }
  swift_delete_context(&c);
}
END_TEST                               


START_TEST (test_objects_premove_verify) {

  struct swift_context *c;
  struct swift_transfer_handle *h;
  swift_error e;
  int object_num;
  char objname[20];
  unsigned char tbuf[64];
  size_t readlen;
  size_t totbytes;
  int curbyte;

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);

  for (object_num = 0; object_num < 50; ++object_num) {
    sprintf(objname, "testobj%02d", object_num);
    /*Test container 1*/
    fail_unless(swift_read_object(c, "testcont00", objname, &h) 
        == SWIFT_SUCCESS);
    fail_unless(h->length == 1024);

    totbytes = 0;
    while (readlen = swift_read(h, tbuf, 64)) {
      totbytes += readlen;
      for (curbyte = 0; curbyte < readlen; ++curbyte) {
        fail_unless(tbuf[curbyte] == 0xAA);
      }
    }

    fail_unless(totbytes == 1024);
    swift_free_transfer_handle(&h);
  }
  swift_delete_context(&c);

}
END_TEST


START_TEST (test_objects_delete) {
  
  struct swift_context *c;
  swift_error e;
  int object_num;
  char objname[20];

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);

  for (object_num = 0; object_num < 50; ++object_num) {
    sprintf(objname, "testobj%02d", object_num);
    fail_unless(swift_delete_object(c, "testcont00", objname)
        == SWIFT_SUCCESS);
    fail_unless(swift_delete_object(c, "testcont01", objname)
        == SWIFT_SUCCESS); 
  }
  swift_delete_context(&c);
}
END_TEST

START_TEST (test_container_delete) {

  struct swift_context *c;
  swift_error e;
  int container_num;
  char contname[20];

  fail_if(swift_create_context(&c, url, username, password) != SWIFT_SUCCESS);
  
  for (container_num = 99; container_num >= 0; --container_num) {
    sprintf(contname, "testcont%02d", container_num);
    e = swift_delete_container(c, contname);
    fail_unless(e == SWIFT_SUCCESS);
  }
  swift_delete_context(&c);

}
END_TEST


Suite *
swift_integration(void) {
  
  Suite *s = suite_create("libswift");
  TCase *tc_int = tcase_create("Integration Tests");

  tcase_add_test(tc_int, test_reset);
  tcase_add_test(tc_int, test_container_create);
  tcase_add_test(tc_int, test_container_overwrite);
  tcase_add_test(tc_int, test_objects_create);
  tcase_add_test(tc_int, test_objects_premove_verify);
  tcase_add_test(tc_int, test_objects_delete);
  tcase_add_test(tc_int, test_container_delete);

  tcase_set_timeout(tc_int, 100);
  suite_add_tcase(s, tc_int);

  return s;
}

int 
main(int argc, char **argv) {

  char *info;
  int n_failed;
  struct swift_context *c;
  swift_error e;

  info = (char *)malloc(strlen(INTEGRATION) + 1);
  strcpy(info, INTEGRATION);

  if (!parse_info(info, &username, &password, &url)) {
    exit(EXIT_FAILURE);
  }
  printf("Testing with user %s against server: %s\n", username, url);

  swift_init();
  e = swift_create_context(&c, url, username, password);
  if (e) {
    printf("Error creating context!\n");
    exit(EXIT_FAILURE);
  }
  e = swift_can_connect(c);
  if (e) {
    printf("Can't connect to test server! Are you sure it was specified?\n");
    exit(EXIT_FAILURE);
  }

  swift_delete_context(&c);


  Suite *s = swift_integration();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);
  n_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;

}
