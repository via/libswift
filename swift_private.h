#ifndef SWIFT_PRIVATE_H
#define SWIFT_PRIVATE_H
      
#ifdef UNITTEST
#define STATIC
#else
#define STATIC static
#endif
      

STATIC void swift_chomp(char *);
STATIC swift_error swift_response(int);
STATIC struct curl_slist *swift_set_headers(CURL *, int, ...);
STATIC void swift_string_to_list(char *, int, char ***);

#endif
