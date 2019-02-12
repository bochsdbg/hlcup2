#pragma once

#include <cassert>
#include <cstring>
#include <limits>
#include <vector>

#include "common.hpp"
#include "core/SmallString.hpp"

namespace hlcup {

struct Account {
    const static constexpr u32 kInvalidOffset = std::numeric_limits<u32>::max();
    const static constexpr u32 kInvalidId     = std::numeric_limits<u32>::max();

    enum Sex : u8 {
        kFemale     = 0,
        kMale       = 1,
        kInvalidSex = std::numeric_limits<u8>::max(),
    };

    enum Status : u8 {
        kFree          = 0,
        kComplicated   = 1,
        kOccupied      = 2,
        kInvalidStatus = std::numeric_limits<u8>::max(),
    };

    struct Like {
        u32 to_id;
        i32 ts;
    };

    char *string_data;

    u32    id;
    Sex    sex;
    Status status;

    Timestamp joined;
    Timestamp birth;

    struct {
        Timestamp start, finish;
    } premium;

    StringRef fname, sname, country, city, phone, email;

    std::vector<u32>  interests;
    std::vector<Like> likes;

    inline bool addLike(const Like &like) {
        likes.push_back(like);
        return true;
    }

    Account() {
        string_data = reinterpret_cast<char *>(::malloc(8192));
        assert(string_data != nullptr);
        clear();
    }

    size_t getInterestsCount() const { return interests.size() <= 1 ? 0 : interests.size() - 1; }

    StringRef getInterest(size_t idx) const { return StringRef{interests[idx], interests[idx + 1] - interests[idx]}; }

    void clear() {
        id     = kInvalidId;
        sex    = kInvalidSex;
        status = kInvalidStatus;
        joined = birth = kInvalidTimestamp;
        premium.start = premium.finish = kInvalidTimestamp;

        fname.offset = sname.offset = country.offset = city.offset = phone.offset = email.offset = kInvalidOffset;
        interests.clear();
        likes.clear();
    }

    ~Account() { free(string_data); }

    inline std::string_view getView(const StringRef &ref) const { return std::string_view(string_data + ref.offset, ref.size); }
    inline std::string      getString(const StringRef &ref) const { return std::string(string_data + ref.offset, ref.size); }
};

}  // namespace hlcup
