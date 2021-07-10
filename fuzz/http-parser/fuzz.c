#include "../../src/http_p.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define INPUTSIZE 2048

struct http_header headers[16] = {0};

int main(int argc, char* argv[]) {
  char input[INPUTSIZE] = {0};
	ssize_t red = read(STDIN_FILENO, input, INPUTSIZE);
  if(red < 1) return 0;
  for(unsigned long i = 0; i < red - 1; ++i) {
    if(input[i] == '\\') {
      if(input[i + 1] == 'r') {
        input[i] = '\r';
        memmove(input + i + 1, input + i + 2, INPUTSIZE - i - 2);
      } else if(input[i + 1] == 'n') {
        input[i] = '\n';
        memmove(input + i + 1, input + i + 2, INPUTSIZE - i - 2);
      } else if(input[i + 1] == 't') {
        input[i] = '\t';
        memmove(input + i + 1, input + i + 2, INPUTSIZE - i - 2);
      }
    }
  }
  struct http_parser_session session;
  (void) memset(&session, 0, sizeof(session));
  struct http_parser_settings settings;
  (void) memset(&settings, 0, sizeof(settings));
  struct http_request request;
  (void) memset(&request, 0, sizeof(request));
  settings.max_method_len = 16;
  settings.max_headers = 16;
  settings.max_path_len = 32;
  settings.max_header_value_len = 32;
  settings.max_header_name_len = 32;
  settings.max_body_len = 64;
  
  request.headers = headers;
  request.headers_len = 16;
  
  (void) http1_parse_request(input, red - 1, &session, &settings, &request);
  return 0;
}