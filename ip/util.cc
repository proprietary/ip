#include "ip/util.h"
#include <glog/logging.h>

namespace net_zelcon::ip {

int make_socket_nonblocking(int socket) {
  const int opts = fcntl(socket, F_GETFL);
  if (opts < 0) {
    auto err = errno;
    PLOG(ERROR) << ("fcntl(F_GETFL)");
    return err;
  }
  if (fcntl(socket, F_SETFL, opts | O_NONBLOCK) < 0) {
    auto err = errno;
    PLOG(ERROR) << "fcntl(F_SETFL)";
    return err;
  }
  return 0;
}

} // namespace net_zelcon::ip
