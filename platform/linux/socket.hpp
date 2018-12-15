#pragma once

#include <sys/socket.h>

#include "syscall.hpp"

namespace ef {
namespace platform {

int socket(int domain, int type, int protocol) { return syscall<int>(SC::socket, domain, type, protocol); }
int socketpair(int domain, int type, int protocol, int fds[2]) { return syscall<int>(SC::socketpair, domain, type, protocol, fds); }

int shutdown(int fd, int how) { return syscall<int>(SC::shutdown, fd, how); }

int bind(int fd, const struct sockaddr *umyaddr, socklen_t addrlen) { return syscall<int>(SC::bind, fd, umyaddr, addrlen); }
int connect(int fd, const struct sockaddr *uservaddr, socklen_t addrlen) { return syscall<int>(SC::connect, fd, uservaddr, addrlen); }
int listen(int fd, int backlog) { return syscall<int>(SC::listen, fd, backlog); }
int accept(int fd, sockaddr *__restrict upeer_sockaddr, socklen_t *__restrict upeer_addrlen) {
    return syscall<int>(SC::accept, fd, upeer_sockaddr, upeer_addrlen);
}
int accept4(int fd, struct sockaddr *__restrict upeer_sockaddr, socklen_t *__restrict upeer_addrlen, int flags) {
    return syscall<int>(SC::accept4, fd, upeer_sockaddr, upeer_addrlen, flags);
}

int getsockname(int fd, struct sockaddr *__restrict usockaddr, socklen_t *__restrict usockaddr_len) {
    return syscall<int>(SC::getsockname, fd, usockaddr, usockaddr_len);
}
int getpeername(int fd, struct sockaddr *__restrict usockaddr, socklen_t *__restrict usockaddr_len) {
    return syscall<int>(SC::getpeername, fd, usockaddr, usockaddr_len);
}

ssize_t send(int, const void *, size_t, int);
ssize_t recv(int, void *, size_t, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t recvfrom(int, void *__restrict, size_t, int, struct sockaddr *__restrict, socklen_t *__restrict);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t recvmsg(int, struct msghdr *, int);

int getsockopt(int, int, int, void *__restrict, socklen_t *__restrict);
int setsockopt(int, int, int, const void *, socklen_t);

int sockatmark(int);

}  // namespace platform
}  // namespace ef
