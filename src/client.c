#include <stdio.h>
#include <string.h>

#include "swift.h"

swift_error swift_authenticate(struct swift_context *s);

int
main(int argc, char ** argv) {

  struct swift_context *c = NULL;
  swift_error e;
  int l;
  char ** contents = NULL;
  unsigned long size;
  void *data;

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
      printf("Found, size = %ld\n", size);
    }
  } else if (strcmp(argv[1], "readobj") == 0) {
    struct swift_transfer_handle *h;
    if (swift_read_object(c, argv[2], argv[3], &h)) {
      printf ("Error\n");
    } else {
      swift_get_data(h, &data);
      printf("%100s\n", (char *)data);
      swift_free_transfer_handle(&h);
    }
  } else if (strcmp(argv[1], "writeobj") == 0) {
    struct swift_transfer_handle *h;
    if (swift_create_object(c, argv[2], argv[3], &h, 100)) {
      printf("Error\n");
    } else {
      swift_get_data(h, &data);
      strcpy(data, "Test one two three");
      swift_sync(h);
      swift_free_transfer_handle(&h);
    }
  }

  swift_delete_context(&c);

  return 0;

}
