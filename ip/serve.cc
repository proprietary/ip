#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

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

void run_server(epoll_event *events, size_t maxevents, int server_sock,
                sockaddr_in6 *server_addr) {
  int nfds = 0;
  int client_sock = 0;
  char message[1024];
  char client_ip[INET6_ADDRSTRLEN];
  sockaddr_in6 client_addr{};
  socklen_t client_addr_len = sizeof(client_addr);

  // Bind
  if (bind(server_sock, reinterpret_cast<const sockaddr *>(server_addr),
           sizeof(*server_addr)) < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen
  constexpr int backlog = 100; // TODO(zds): make `backlog` configurable
  if (auto err = listen(server_sock, backlog); err < 0) {
    perror("listen");
  }

  // Create epoll
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
  }
  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = server_sock;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &ev) < 0) {
    perror("epoll_ctl: EPOLL_CTL_ADD, server_sock");
    exit(EXIT_FAILURE);
  }

  while (true) {
    nfds = epoll_wait(epoll_fd, events, maxevents, -1);
    for (auto n = 0; n < nfds; ++n) {
      if (events[n].data.fd == server_sock) {
        client_sock =
            accept(server_sock, reinterpret_cast<sockaddr *>(&client_addr),
                   &client_addr_len);
        if (auto err = make_socket_nonblocking(client_sock); err < 0) {
          exit(1);
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
            perror("inet_ntop() for ipv4");
          }
        } else {
          auto dst =
              inet_ntop(AF_INET6, &client_addr, static_cast<char *>(client_ip),
                        INET6_ADDRSTRLEN);
          if (dst == nullptr) {
            perror("inet_ntop() for ipv6");
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

int main(int argc, char *argv[]) {
  sockaddr_in6 server_addr{};

  // Create socket
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_addr = in6addr_any;
  server_addr.sin6_port = htons(8888); // TODO(zds): make port configurable
  int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
  if (server_sock == -1) {
    std::cout << "Could not create socket";
  }
  if (auto err = make_socket_nonblocking(server_sock); err < 0) {
    return 1;
  }

  constexpr size_t maxevents = 100; // TODO(zds): make maxevents configurable
  epoll_event events[maxevents];

  // handle OS signals
  // interrupt
  signal(
      SIGINT, +[](int signum) {
        // for (auto i = 0; i < nfds; ++i) {
        //   close(events[i].data.fd);
        // }
        exit(EXIT_SUCCESS);
      });

  run_server(events, maxevents, server_sock, &server_addr);

  return EXIT_SUCCESS;
}
