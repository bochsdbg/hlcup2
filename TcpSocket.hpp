#pragma once

#include <netdb.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include <cstring>

namespace hlcup {

struct TcpSocket {
    TcpSocket(int fd) : fd_(fd) {}
    TcpSocket() { fd_ = ::socket(PF_INET, SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP); }
    TcpSocket(int domain, int type, int proto) {
        fd_ = ::socket(domain, type, proto);
        if (fd_ == -1) { err_code_ = errno; }
    }

    int fd() const { return fd_; }

#ifdef HLCUP_USE_SYSCALL
    long close() { HLCUP_HANDLE_ERR(::syscall(SYS_close, fd_)); }
    long accept4(sockaddr *addr, socklen_t *len, int flags) { HLCUP_HANDLE_ERR(::syscall(SYS_accept4, fd_, addr, len, flags)); }
    long read(void *data, size_t size) { HLCUP_HANDLE_ERR(::syscall(SYS_read, fd_, data, size)); }
    long shutdown() { HLCUP_HANDLE_ERR(::syscall(SYS_shutdown, fd_, SHUT_RDWR)); }
    long write(const void *data, size_t size) { HLCUP_HANDLE_ERR(::syscall(SYS_write, fd_, data, size)); }
    long writev(const iovec *data, size_t size) { HLCUP_HANDLE_ERR(::syscall(SYS_writev, fd_, data, size)); }
    long sendmsg(const MsgHdr &msg, int flags) { HLCUP_HANDLE_ERR(::syscall(SYS_sendmsg, fd_, &msg, flags)); }
    long recvmsg(MsgHdr *msg, int flags) { HLCUP_HANDLE_ERR(::syscall(SYS_recvmsg, fd_, msg, flags)); }
#else
    long close() { HLCUP_HANDLE_ERR(::close(fd_)); }
    long accept4(sockaddr *addr, socklen_t *len, int flags) { HLCUP_HANDLE_ERR(::accept4(fd_, addr, len, flags)); }
    long read(void *data, size_t size) { HLCUP_HANDLE_ERR(::read(fd_, data, size)); }
    long write(const void *data, size_t size) { HLCUP_HANDLE_ERR(::write(fd_, data, size)); }
    long writev(const iovec *data, size_t size) { HLCUP_HANDLE_ERR(::writev(fd_, data, size)); }
    long sendmsg(const MsgHdr &msg, int flags) { HLCUP_HANDLE_ERR(::sendmsg(fd_, &msg, flags)); }
    long recvmsg(MsgHdr *msg, int flags) { HLCUP_HANDLE_ERR(::recvmsg(fd_, msg, flags)); }
#endif

    long bind(sockaddr *addr, socklen_t len) { HLCUP_HANDLE_ERR(::bind(fd_, addr, len)); }
    long bind(sockaddr_in *addr) { return bind(reinterpret_cast<sockaddr *>(addr), sizeof(sockaddr_in)); }
    long bind(uint16_t port) {
        sockaddr_in addr;
        addr.sin_port   = htons(port);
        addr.sin_family = PF_INET;
        std::memset(addr.sin_zero, 0, get_size(addr.sin_zero));
        addr.sin_addr.s_addr = INADDR_ANY;  // htonl(INADDR_LOOPBACK);
        return bind(&addr);
    }

    long listen(int n) { HLCUP_HANDLE_ERR(::listen(fd_, n)); }

    long accept(sockaddr_in *addr) {
        socklen_t len;
        return accept4(reinterpret_cast<sockaddr *>(addr), &len, SOCK_CLOEXEC | SOCK_NONBLOCK);
    }

    long accept(int flags = SOCK_CLOEXEC | SOCK_NONBLOCK) { return accept4(nullptr, nullptr, flags); }

    template <typename T>
    long setoption(int level, int optname, const T &val) {
        HLCUP_HANDLE_ERR(::setsockopt(fd_, level, optname, &val, sizeof(T)));
    }

    long writev(const std::vector<IoVec> &data) { return writev(data.data(), data.size()); }

    int lastError() const { return err_code_; }

private:
    int fd_;
    int err_code_ = 0;
};

}  // namespace hlcup
