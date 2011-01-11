#include "../src/swift.h"
#include "curl/curl.h"

#ifndef INTEGRATION
#error You must specify --enable-integration=user,password,url
#endif

const char *info = INTEGRATION;

void parse_info(char *info, char **username, char **password, char **url) {

  *username = strchr(info, ',');
  *password = strchr(*username, ',');
  *url = strchr(*password, ',');

}

int main(int argc, char **argv) {

  return 0;

}
