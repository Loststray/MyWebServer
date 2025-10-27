#include "config.hpp"
#include <cstdlib>
#include <unistd.h>
namespace Web {

Config::Config() {
  PORT = 9999;
  TRIGMode = 0;
  OPT_LINGER = 0;
  sql_num = 8;
  thread_num = 8;
  close_log = false;
  log_queue_size = 1024;
}

void Config::parse_arg(int argc, char *argv[]) {
  int opt;
  const char *str = "p:m:o:s:t:c:q:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
    case 'p': {
      PORT = atoi(optarg);
      break;
    }
    case 'm': {
      TRIGMode = atoi(optarg);
      break;
    }
    case 'o': {
      OPT_LINGER = atoi(optarg);
      break;
    }
    case 's': {
      sql_num = atoi(optarg);
      break;
    }
    case 't': {
      thread_num = atoi(optarg);
      break;
    }
    case 'c': {
      close_log = atoi(optarg);
      break;
    }
    case 'q': {
      log_queue_size = atoi(optarg);
      break;
    }
    default:
      break;
    }
  }
}
} // namespace Web