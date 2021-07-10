#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http_p.h>
#include <shnet/compress.h>

struct http_message message;
struct http_parser_session session;
struct http_parser_settings settings;

struct http_header headers[8];

int cleaned = 0;

void cleanup(void) {
  cleaned = 1;
  
  if(message.allocated_body) {
    free(message.body);
  }
  memset(&message, 0, sizeof(message));
  memset(&session, 0, sizeof(session));
  memset(&settings, 0, sizeof(settings));
  memset(headers, 0, sizeof(headers));
  
  /* Pretty normal settings to start off with */
  
  settings.max_method_len = 64;
  settings.max_headers = 64;
  settings.max_path_len = 64;
  settings.max_header_value_len = 64;
  settings.max_header_name_len = 64;
  settings.max_body_len = 64;
  
  message.headers = headers;
  message.headers_len = 8;
}

void test_explicit(char* const name, char* const buf, const int expected, const int clean, const unsigned length) {
  if(cleaned == 0) {
    cleaned = 1;
    cleanup();
  }
  const int res = http1_parse_request(buf, length, &session, &settings, &message);
  if(res != expected) {
    printf_debug("Test suite %s yielded unexpected value: %s", 1, name, http1_parser_strerror(res));
    TEST_FAIL;
  } else {
    printf_debug("Test suite %s passed", 1, name);
    cleaned = 0;
    if(clean == 1) {
      cleanup();
    }
  }
}

void test(char* const name, char* const buf, const int expected, const int clean) {
  test_explicit(name, buf, expected, clean, strlen(buf));
}

void test_addon(char* const name, char* const buf, const int expected, const int clean, const unsigned addon) {
  test_explicit(name, buf, expected, clean, strlen(buf) + addon);
}

#define check(a,b) do { if((a) != (b)) {TEST_FAIL;} } while(0)
#define not_check(a,b) do { if((a) == (b)) {TEST_FAIL;} } while(0)
#define cmp(a,b) check(strncmp((a), (b), strlen((b))), 0)

int main() {
  printf_debug("Testing http_p:", 1);
  
  /* Method */
  test("1", "", http_incomplete, 1);
  test("2", "A ", http_incomplete, 1);
  settings.max_method_len = 0;
  test("3", "A ", http_method_too_long, 1);
  test("4", "  ", http_no_method, 1);
  test("5", "GET ", http_incomplete, 0);
  cmp(message.method, "GET");
  cleanup();
  settings.stop_at_method = 1;
  test("6", "GET ", http_valid, 0);
  cmp(message.method, "GET");
  
  /* Path */
  test("7", "GET  ", http_no_path, 1);
  test("8", "GET / ", http_incomplete, 0);
  cmp(message.path, "/");
  cleanup();
  settings.max_path_len = 0;
  test("9", "GET / ", http_path_too_long, 1);
  settings.stop_at_path = 1;
  test("10", "GET /t%20o/./../index.html ", http_valid, 0);
  cmp(message.path, "/t%20o/./../index.html");
  
  /* HTTP Version */
  test("11", "GET / HTTP/1.1\r\n", http_incomplete, 1);
  test("12", "GET / H TP/1.1\r\n", http_invalid_version, 1);
  test("13", "GET / HTTP/1.2\r\n", http_invalid_version, 1);
  test("14", "GET / HTTP/1.1ab", http_incomplete, 1);
  settings.stop_at_version = 1;
  test("15", "GET / HTTP/1.1  ", http_valid, 1);
  test("15", "GET /  HTTP/1.1  ", http_invalid_version, 1);
  
  /* Headers */
  test("17",
  "GET / HTTP/1.1\r\n"
  "Name: Value\r\n"
  , http_incomplete, 0);
  cmp(message.headers[0].name, "Name");
  cmp(message.headers[0].value, "Value");
  test("18",
  "GET / HTTP/1.1\r\n"
  "Name:  Value\r\n"
  , http_invalid_character, 1);
  test("19",
  "GET / HTTP/1.1\r\n"
  "Name : Value\r\n"
  , http_invalid_character, 1);
  test("20",
  "GET / HTTP/1.1\r\n"
  "Name:Value\r\n"
  , http_incomplete, 0);
  cmp(message.headers[0].value, "alue");
  test("21",
  "GET / HTTP/1.1\r\n"
  ": Value\r\n"
  , http_no_header_name, 1);
  test("22",
  "GET / HTTP/1.1\r\n"
  "Name:\r\n"
  , http_incomplete, 1);
  test("23",
  "GET / HTTP/1.1\r\n"
  "Name:\r\n\n"
  , http_invalid_character, 1);
  test("24",
  "GET / HTTP/1.1\r\n"
  "a:ab\r\n"
  , http_incomplete, 0);
  cmp(message.headers[0].name, "a");
  cmp(message.headers[0].value, "b");
  cleanup();
  settings.stop_at_headers = 1;
  settings.stop_at_every_header = 1;
  test("25",
  "GET / HTTP/1.1\r\n"
  "Name: Value\r\n"
  "SecondName: SecondValue\r\n"
  , http_valid, 0);
  cmp(message.headers[0].name, "Name");
  cmp(message.headers[0].value, "Value");
  cleaned = 1;
  test("26",
  "GET / HTTP/1.1\r\n"
  "Name: Value\r\n"
  "SecondName: SecondValue\r\n"
  , http_valid, 0);
  cmp(message.headers[1].name, "SecondName");
  cmp(message.headers[1].value, "SecondValue");
  cleanup();
  settings.max_header_name_len = 0;
  test("27",
  "GET / HTTP/1.1\r\n"
  "Name: Value\r\n"
  , http_header_name_too_long, 1);
  settings.max_header_value_len = 0;
  test("28",
  "GET / HTTP/1.1\r\n"
  "Name: Value\r\n"
  , http_header_value_too_long, 1);
  settings.max_header_value_len = 0;
  test("29",
  "GET / HTTP/1.1\r\n"
  "Name: \r\n"
  , http_incomplete, 1);
  test("30",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 5\r\n"
  , http_incomplete, 0);
  check(message.body_len, 5);
  test("31",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 9999999999\r\n"
  , http_body_too_long, 1);
  test("32",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 999\r\n"
  , http_body_too_long, 1);
  test("33",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  , http_incomplete, 0);
  check(message.transfer, http_t_chunked);
  test("34",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: weird_value\r\n"
  , http_transfer_not_supported, 1);
  test("35",
  "GET / HTTP/1.1\r\n"
  "Content-Encoding: br\r\n"
  , http_incomplete, 0);
  check(message.encoding, http_e_brotli);
  test("36",
  "GET / HTTP/1.1\r\n"
  "Content-Encoding: deflate\r\n"
  , http_incomplete, 0);
  check(message.encoding, http_e_deflate);
  test("37",
  "GET / HTTP/1.1\r\n"
  "Content-Encoding: a\r\n"
  , http_encoding_not_supported, 1);
  test("38",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 18\r\n"
  "\r\nSome text I wrote."
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("39",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 18\r\n"
  "\r\nSome text I wrote"
  , http_incomplete, 1);
  test("40",
  "GET / HTTP/1.1\r\n"
  "Content-Length: 17\r\n"
  "\r\nSome text I wrote."
  , http_valid, 0);
  check(message.body_len, 17);
  cmp(message.body, "Some text I wrote");
  test("41",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("42",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0"
  , http_incomplete, 1);
  test("43",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r"
  , http_incomplete, 0);
  cleaned = 1;
  test("44",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("45",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\r"
  , http_valid, 1);
  test("46",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote"
  , http_incomplete, 0);
  cleaned = 1;
  test("47",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("48",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r"
  , http_incomplete, 0);
  cleaned = 1;
  test("49",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("50",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7"
  , http_incomplete, 0);
  cleaned = 1;
  test("51",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  test("52",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n"
  , http_incomplete, 0);
  cleaned = 1;
  test("53",
  "GET / HTTP/1.1\r\n"
  "Transfer-Encoding: chunked\r\n"
  "\r\na\r\nSome text \r\n7\r\nI wrote\r\n1\r\n.\r\n0\r\n"
  , http_valid, 0);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  
  char buff[512];
  memset(buff, 0, sizeof(buff));
  size_t size;
  void* compressed = deflate_compress("Some text I wrote.", 18, &size, Z_BEST_COMPRESSION);
  not_check(compressed, NULL);
  int len = sprintf(buff, "GET / HTTP/1.1\r\n"
  "Content-Length: %lu\r\n"
  "Content-Encoding: deflate\r\n"
  "\r\n", size);
  memcpy(buff + len, compressed, size);
  
  test_explicit("54", buff, http_valid, 0, len + size);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  
  memset(buff, 0, sizeof(buff));
  compressed = brotli_compress("Some text I wrote.", 18, &size, BROTLI_MAX_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_TEXT);
  not_check(compressed, NULL);
  len = sprintf(buff, "GET / HTTP/1.1\r\n"
  "Content-Length: %lu\r\n"
  "Content-Encoding: br\r\n"
  "\r\n", size);
  memcpy(buff + len, compressed, size);
  
  test_explicit("55", buff, http_valid, 0, len + size);
  check(message.body_len, 18);
  cmp(message.body, "Some text I wrote.");
  
  TEST_PASS;
  return 0;
}