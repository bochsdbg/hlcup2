#pragma once

#include <sys/types.h>
#include <cstddef>
#include <cstdint>

#include "syscall.hpp"

namespace ef {
namespace platform {

static const constexpr int kStdinFileno  = 0;
static const constexpr int kStdoutFileno = 1;
static const constexpr int kStderrFileno = 2;

[[maybe_unused]] static inline long read(int fd, void *buf, size_t size) { return syscall<long>(SC::read, fd, buf, size); }
[[maybe_unused]] static inline long write(int fd, const void *buf, size_t size) { return syscall<long>(SC::write, fd, buf, size); }
[[maybe_unused]] static inline long pread(int fd, void *buf, size_t size, off_t offset) { return syscall<long>(SC::pread64, fd, buf, size, offset); }
[[maybe_unused]] static inline long pwrite(int fd, const void *buf, size_t size, off_t offset) { return syscall<long>(SC::pwrite64, fd, buf, size, offset); }

[[maybe_unused]] static inline int close(int fd) { return syscall<int>(SC::close, fd); }

}  // namespace platform
}  // namespace ef
