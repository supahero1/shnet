#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

#include "consts.h"

#include <shnet/time.h>
#include <shnet/version.h>

static uint32_t FNV_1a(const char* const str, const uint32_t len) {
  uint32_t ret = 0x811c9dc5;
  for(uint32_t i = 0; i < len; ++i) {
    ret ^= str[i];
    ret *= 0x01000193;
  }
  return ret;
}

static const char* const help =
"Usage: %s METHOD [OPTIONS]\n"
"\n"
"core\n"
"   Methods\n"
"\th ? help        Display this message\n"
"\tv version       Get the CLI's version\n"
"   Options\n"
"\tf force         If the CLI doesn't want to execute a command due to\n"
"\t                the input being ridiculous, this will do the trick."
"\n"
"time\n"
"   Methods\n"
"\ttime-bench      Benchmark the time module\n"
"   Options\n"
"\tn num           Number of timers to use\n"
"\tfast            Do not benchmark POSIX and thread timers\n"
"\n"
"You cannot use the \"=\" notation to provide arguments. Instead, provide any\n"
"arguments right after the option that requires them, with a space in between.\n"
"Example usage: %s time-bench n 1000 fast\n"
;

enum cli_option_type {
  cli_option_help,
  cli_option_version,

  cli_option_num,
  cli_option_fast,
  cli_option_force,

  cli_option_time_bench
};

struct cli_option {
  char* name;
  enum cli_option_type option;
};

static struct cli_option cli_options[] = {
  { NULL, 0 },
  { "h", cli_option_help },
  { "?", cli_option_help },
  { "help", cli_option_help },
  { "v", cli_option_version },
  { "version", cli_option_version },
  
  { "n", cli_option_num },
  { "num", cli_option_num },
  { "fast", cli_option_fast },
  { "f", cli_option_force },
  { "force", cli_option_force },

  { "time-bench", cli_option_time_bench }
};

#define cli_options_len (sizeof(cli_options) / sizeof(cli_options[0]))

enum cli_method {
  cli_method_invalid,
  cli_method_time_bench
};

struct options options = {0};

static char whitespace[] = "\r                                                                                ";

void print(const char* const restrict format, ...) {
  if(format[0] == '\r') {
    printf("%.80s", whitespace);
  }
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  if(format[0] == '\r') {
    fflush(stdout);
  }
}

void print_time(const uint64_t n) {
  if(time_us_to_ns(1) > n) {
    printf("%" PRIu64 "ns", n);
  } else if(time_ms_to_ns(1) > n) {
    printf("%.1lfus", (double) n / time_us_to_ns(1));
  } else if(time_sec_to_ns(1) > n) {
    printf("%.1lfms", (double) n / time_ms_to_ns(1));
  } else {
    printf("%.1lfsec", (double) n / time_sec_to_ns(1));
  }
}

#define ARGS(x, y) \
do { \
  if(i + x >= argc) { \
    printf("Missing argument%s for option \"%s\". Expected: %s\n", x > 1 ? "s" : "", argv[i], y); \
    goto out; \
  } \
} while(0)

int main(int argc, char** argv) {
  if(argc <= 1) {
    if(argc == 1) {
      printf(help, argv[0], argv[0]);
    }
    return 0;
  }
  uint32_t* const hash_table = calloc(cli_options_len << 1, sizeof(*hash_table));
  assert(hash_table);
  for(uint32_t i = 1; i < cli_options_len; ++i) {
    uint32_t idx = FNV_1a(cli_options[i].name, strlen(cli_options[i].name)) % (cli_options_len << 1);
    while(hash_table[idx] != 0) {
      idx = (idx + 1) % (cli_options_len << 1);
    }
    hash_table[idx] = i;
  }
  enum cli_method method = 0;
  for(int i = 1; i < argc; ++i) {
    const uint32_t len = strlen(argv[i]);
    uint32_t idx = FNV_1a(argv[i], len) % (cli_options_len << 1);
    while(1) {
      if(hash_table[idx] == 0) {
        printf("Unrecognized option \"%s\".\n", argv[i]);
        goto out;
      }
      if(len == strlen(cli_options[hash_table[idx]].name) && strncmp(cli_options[hash_table[idx]].name, argv[i], len) == 0) {
        break;
      }
      idx = (idx + 1) % (cli_options_len << 1);
    }
    switch(cli_options[hash_table[idx]].option) {
      case cli_option_help: {
        printf(help, argv[0], argv[0]);
        goto out;
      }
      case cli_option_version: {
        printf("shnet CLI %u.%u | Copyright (C) 2022 | Apache License 2.0\n", SHNET_CLI_VERSION, SHNET_CLI_PATCH);
        goto out;
      }
      case cli_option_time_bench: {
        method = cli_method_time_bench;
        break;
      }
      case cli_option_num: {
        ARGS(1, "number");
        options.num = atoi(argv[++i]);
        break;
      }
      case cli_option_fast: {
        options.fast = 1;
        break;
      }
      case cli_option_force: {
        options.force = 1;
        break;
      }
      default: assert(0);
    }
  }
  switch(method) {
    case cli_method_invalid: {
      printf("You have not chosen any method. If you are unsure what they are, try:\n\n$> %s h\n\nto get more information. An example of a valid method is \"help\" or \"version\".\n", argv[0]);
      break;
    }
    case cli_method_time_bench: {
      time_benchmark();
      break;
    }
  }

  out:;
  free(hash_table);
  return 0;
}

#undef ARGS
#undef cli_options_len
