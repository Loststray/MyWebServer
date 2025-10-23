#include "Mysocket.hpp"
#include <sys/socket.h>
#include <unistd.h>

namespace Web {
Mysocket::Mysocket(int domain, int type) { fd_ = socket(domain, type, 0);}
Mysocket::~Mysocket() { close(fd_); }
} // namespace Web