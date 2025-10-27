#include "config.hpp"
#include "tcp_server.hpp"
#include <bits/stdc++.h>
int main(int argc, char *argv[]) {
  Web::Config config;
  config.parse_arg(argc, argv);
  Web::WebServer server(config.PORT, config.TRIGMode, 5000, config.OPT_LINGER,
                        "db.sqlite3", config.sql_num, config.thread_num,
                        config.close_log, config.log_queue_size);
  server.Start();
  return 0;
}