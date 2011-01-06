#include <stdio.h>

#include "swift.h"

swift_error swift_authenticate(struct swift_context *s);

int
main(int argc, char ** argv) {



  struct swift_context *c = NULL;
  swift_error e;
  int l;
  char ** contents = NULL;
  unsigned long size;

  swift_init();
  e = swift_create_context(&c, "http://swiftbox:11000/v1.0", "test:tester", "testing");
  if (e) {
    printf ("Error!\n");
  }
  if (strcmp(argv[1], "nodelist") == 0) {
    e = swift_node_list(c, argv[2], &l, &contents);
    if (e) {
      printf("Error: %d\n", e);
    } else {
      char *str;
      int pos;
      for (pos = 0, str = contents[pos]; str != NULL; str = contents[++pos]) {
        printf("%s\n", contents[pos]);
      }
      swift_node_list_free(&contents);
    }
  } else if (strcmp(argv[1], "contexist") == 0) {
    if (swift_container_exists(c, argv[2])) {
      printf ("Does not exist!\n");
    } else {
      printf ("%s exists.\n", argv[2]);
    }
  } else if (strcmp(argv[1], "contcreate") == 0) {
    if (swift_create_container(c, argv[2])) {
      printf ("Failed\n");
    }
  } else if (strcmp(argv[1], "contdelete") == 0) {
    if (swift_delete_container(c, argv[2])) {
      printf("Failed\n");
    }
  } else if (strcmp(argv[1], "objexist") == 0) {
    if (swift_object_exists(c, argv[2], argv[3], &size)) {
      printf("Not found!\n");
    } else {
      printf("Found, size = %d\n", size);
    }
  }

  swift_delete_context(&c);

}
