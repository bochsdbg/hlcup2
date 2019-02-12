#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define HLCUP_LIKELY(x) __builtin_expect(!!(x), 1)
#define HLCUP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define HLCUP_ALWAYS_INLINE __attribute__((always_inline))
#define HLCUP_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define HLCUP_LIKELY(x) x
#define HLCUP_UNLIKELY(x) x
#define HLCUP_ALWAYS_INLINE __forceinline
#define HLCUP_UNREACHABLE() __assume(0)
#else
#define HLCUP_LIKELY(x) x
#define HLCUP_UNLIKELY(x) x
#define HLCUP_ALWAYS_INLINE inline
#define HLCUP_UNREACHABLE() assert(!"unreachable")
#endif

namespace hlcup {

using i64 = std::int64_t;
using i32 = std::int32_t;
using i16 = std::int16_t;
using i8  = std::int8_t;

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

struct StringRef {
    u32 offset;
    u32 size;

    void set_by_size(u32 offset, u32 size) {
        this->offset = offset;
        this->size   = size;
    }

    void set_by_offset(u32 o1, u32 o2) {
        offset = o1;
        size   = o2 - o1;
    }
};

using Timestamp = i32;

const static constexpr Timestamp kInvalidTimestamp = std::numeric_limits<Timestamp>::max();

enum Sex : u8 {
    kFemale = 0,
    kMale   = 1,
};

enum Status : u8 {
    kFree        = 0,
    kComplicated = 1,
    kOccupied    = 2,
};

}  // namespace hlcup
