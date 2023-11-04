#ifndef _INCLUDE_NET_ZELCON_IP_UTIL_H
#define _INCLUDE_NET_ZELCON_IP_UTIL_H

#include <fcntl.h>

namespace net_zelcon::ip {

  int make_socket_nonblocking(int socket);
 
} // namespace net_zelcon::ip


#endif // _INCLUDE_NET_ZELCON_IP_UTIL_H
