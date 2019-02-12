#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include "syscall.hpp"

namespace ef {
namespace platform {

[[maybe_unused]] static inline int socket(int domain, int type, int protocol) { return syscall<int>(SC::socket, domain, type, protocol); }
[[maybe_unused]] static inline int socketpair(int domain, int type, int protocol, int fds[2]) {
    return syscall<int>(SC::socketpair, domain, type, protocol, fds);
}

[[maybe_unused]] static inline int shutdown(int fd, int how) { return syscall<int>(SC::shutdown, fd, how); }

[[maybe_unused]] static inline int bind(int fd, const sockaddr *addr, socklen_t addrlen) { return syscall<int>(SC::bind, fd, addr, addrlen); }
[[maybe_unused]] static inline int connect(int fd, const sockaddr *addr, socklen_t addrlen) { return syscall<int>(SC::connect, fd, addr, addrlen); }
template <typename T>
[[maybe_unused]] static inline int connect(int fd, const T *addr) {
    return ef::platform::connect(fd, reinterpret_cast<const sockaddr *>(addr), sizeof(T));
}

[[maybe_unused]] static inline int listen(int fd, int backlog) { return syscall<int>(SC::listen, fd, backlog); }
[[maybe_unused]] static inline int accept(int fd, sockaddr *__restrict upeer_sockaddr, socklen_t *__restrict upeer_addrlen) {
    return syscall<int>(SC::accept, fd, upeer_sockaddr, upeer_addrlen);
}
[[maybe_unused]] static inline int accept4(int fd, sockaddr *__restrict upeer_sockaddr, socklen_t *__restrict upeer_addrlen, int flags) {
    return syscall<int>(SC::accept4, fd, upeer_sockaddr, upeer_addrlen, flags);
}

[[maybe_unused]] static inline int getsockname(int fd, sockaddr *__restrict usockaddr, socklen_t *__restrict usockaddr_len) {
    return syscall<int>(SC::getsockname, fd, usockaddr, usockaddr_len);
}
[[maybe_unused]] static inline int getpeername(int fd, sockaddr *__restrict usockaddr, socklen_t *__restrict usockaddr_len) {
    return syscall<int>(SC::getpeername, fd, usockaddr, usockaddr_len);
}

[[maybe_unused]] static inline ssize_t sendto(int fd, const void *buf, size_t len, unsigned int flags, const sockaddr *addr, socklen_t addr_size) {
    return syscall<ssize_t>(SC::sendto, fd, buf, len, flags, addr, addr_size);
}
[[maybe_unused]] static inline ssize_t send(int fd, const void *buf, size_t len, unsigned int flags) { return sendto(fd, buf, len, flags, nullptr, 0); }

[[maybe_unused]] static inline ssize_t recvfrom(int fd, void *__restrict buf, size_t len, int flags, sockaddr *__restrict addr,
                                                socklen_t *__restrict addr_size) {
    return syscall<ssize_t>(SC::recvfrom, fd, buf, len, flags, addr, addr_size);
}
[[maybe_unused]] static inline ssize_t recv(int fd, void *__restrict buf, size_t len, int flags) { return recvfrom(fd, buf, len, flags, nullptr, nullptr); }
[[maybe_unused]] static inline ssize_t sendmsg(int fd, const msghdr *msg, int flags) { return syscall<ssize_t>(SC::sendmsg, fd, msg, flags); }
[[maybe_unused]] static inline ssize_t recvmsg(int fd, msghdr *msg, int flags) { return syscall<ssize_t>(SC::recvmsg, fd, msg, flags); }

[[maybe_unused]] static inline int sendmmsg(int fd, mmsghdr *msgvec, unsigned int vlen, unsigned int flags) {
    return syscall<int>(SC::sendmmsg, fd, msgvec, vlen, flags);
}

[[maybe_unused]] static inline int getsockopt(int fd, int level, int optname, void *__restrict optval, socklen_t *__restrict optlen) {
    return syscall<int>(SC::getsockopt, fd, level, optname, optval, optlen);
}

[[maybe_unused]] static inline int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    return syscall<int>(SC::setsockopt, fd, level, optname, optval, optlen);
}

[[maybe_unused]] static inline int setsockopt(int fd, int level, int optname, int optval) {
    return setsockopt(fd, level, optname, &optval, sizeof (int));
}

}  // namespace platform
}  // namespace ef
