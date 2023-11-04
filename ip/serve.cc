#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fmt/core.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <limits>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

DEFINE_int32(port, 60359, "Port to listen for connections");
DEFINE_validator(
    port, +[](const char *flagname, int32_t value) {
      if (value > std::numeric_limits<uint16_t>::min() &&
          value < std::numeric_limits<uint16_t>::max()) {
        return true;
      }
      fmt::print(stderr, "The port you selected is not a valid port: --{}={}",
                 flagname, value);
      return false;
    });
DEFINE_int32(
    backlog, 100,
    "Receive backlog (If you don't know, just leave this at the default)");
DEFINE_int32(maxevents, 100,
             "`maxevents` to use in epoll (If you don't know, just leave this "
             "at the default)");

int make_socket_nonblocking(int socket) {
  const int opts = fcntl(socket, F_GETFL);
  if (opts < 0) {
    auto err = errno;
    perror("fcntl(F_GETFL)\n");
    return err;
  }
  if (fcntl(socket, F_SETFL, opts | O_NONBLOCK) < 0) {
    auto err = errno;
    perror("fcntl(F_SETFL)\n");
    return err;
  }
  return 0;
}

void run_server(int &num_fds, epoll_event *events, size_t maxevents,
                int server_sock, sockaddr_in6 *server_addr) {
  int client_sock = 0;
  char message[1024];
  char client_ip[INET6_ADDRSTRLEN];
  sockaddr_in6 client_addr{};
  socklen_t client_addr_len = sizeof(client_addr);

  // Bind
  auto err = bind(server_sock, reinterpret_cast<const sockaddr *>(server_addr),
                  sizeof(*server_addr));
  if (err < 0) {
    PLOG(FATAL) << "bind() failed";
  }

  // Listen
  err = listen(server_sock, FLAGS_backlog);
  if (err < 0) {
    PLOG(FATAL) << "listen() failed";
  }

  // Create epoll
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    PLOG(FATAL) << "epoll_create1";
  }
  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = server_sock;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &ev) < 0) {
    PLOG(FATAL) << "epoll_ctl: EPOLL_CTL_ADD, server_sock";
  }

  while (true) {
    num_fds = epoll_wait(epoll_fd, events, maxevents, -1);
    for (auto n = 0; n < num_fds; ++n) {
      if (events[n].data.fd == server_sock) {
        client_sock =
            accept(server_sock, reinterpret_cast<sockaddr *>(&client_addr),
                   &client_addr_len);
        if (auto err = make_socket_nonblocking(client_sock); err < 0) {
          PLOG(FATAL) << "make_socket_nonblocking";
        }
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_sock;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev);
      } else {
        memset(message, 0, sizeof(message));
        memset(&client_ip, 0, sizeof(client_ip));
        if (IN6_IS_ADDR_V4MAPPED(&(&client_addr)->sin6_addr)) {
          auto dst =
              inet_ntop(AF_INET, &client_addr, static_cast<char *>(client_ip),
                        INET6_ADDRSTRLEN);
          if (dst == nullptr) {
            PLOG(FATAL) << "inet_ntop() for ipv4";
          }
        } else {
          auto dst =
              inet_ntop(AF_INET6, &client_addr, static_cast<char *>(client_ip),
                        INET6_ADDRSTRLEN);
          if (dst == nullptr) {
            PLOG(FATAL) << "inet_ntop() for ipv6";
          }
        }
        sprintf(static_cast<char *>(message),
                "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %ld\r\n\r\n%s",
                strlen(static_cast<char *>(client_ip)),
                static_cast<char *>(client_ip));
        write(events[n].data.fd, static_cast<char *>(message),
              strlen(static_cast<char *>(message)));
        close(events[n].data.fd);
      }
    }
  }
}

namespace {

epoll_event *events = new epoll_event[FLAGS_maxevents];
int num_fds = 0;

static void handle_sigint(int signum) {
  LOG(INFO) << "SIGINT detected. Shutting down safely.";
  for (auto i = 0; i < num_fds; ++i) {
    DLOG(INFO) << "Closing file descriptor " << i + 1 << "/" << num_fds + 1;
    close(events[i].data.fd);
  }
  DLOG(INFO) << "Closed all extant file descriptors";
  delete[] events;
  exit(EXIT_SUCCESS);
}

} // namespace

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_logtostderr = true;

  sockaddr_in6 server_addr{};

  // Create socket
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_addr = in6addr_any;
  server_addr.sin6_port =
      htons(FLAGS_port); // TODO(zds): make port configurable
  int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
  if (server_sock == -1) {
    perror("socket");
  }
  if (auto err = make_socket_nonblocking(server_sock); err < 0) {
    return 1;
  }

  // handle OS signals
  // interrupt
  signal(SIGINT, &handle_sigint);

  LOG(INFO) << "Starting server on port " << FLAGS_port << "...";

  run_server(num_fds, events, FLAGS_maxevents, server_sock, &server_addr);

  delete[] events;

  return EXIT_SUCCESS;
}
