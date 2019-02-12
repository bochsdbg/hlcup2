#pragma once

#include "Request.hpp"

namespace hlcup {

struct HttpParser {
    u32 content_length;

    void reset();

    ssize_t parse(const char *p, const char *pe, Request &req);

private:
    union {
        u64       u64_val;
        u32       u32_val;
        i64       i64_val;
        i32       i32_val;
        StringRef sref_val;
    } tmp;
    int   cs;
    char *out_buf;
    u32   offset;
};

}  // namespace hlcup
