#include <stdio.h>

#include "swift.h"

swift_error swift_authenticate(struct swift_context *s);

int
main() {



  struct swift_context *c = NULL;
  swift_error e;
  int l;
  char ** contents = NULL;

  swift_init();
  e = swift_create_context(&c, "http://swift:11000/v1.0", "vikki:tester", "canadia04");
  if (e) {
    printf ("Error!\n");
  }
  swift_node_list(c, "/", &l, &contents);

  swift_delete_context(&c);

}
