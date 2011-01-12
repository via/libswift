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

  for (container--; container_num >= 0; --container_num) {
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

  tcase_add_test(tc_int, test_container_create);

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
  srunner_run_all(sr, CK_NORMAL);
  n_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;

}
