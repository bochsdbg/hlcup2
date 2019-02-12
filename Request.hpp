#pragma once

#include "common.hpp"

#include <bitset>
#include <vector>

#include "Account.hpp"

namespace hlcup {

struct Request {
    enum Type {
        kInvalid = 0,
        kFilter,
        kGroup,
        kRecommend,
        kSuggest,
        kAccountsNew,
        kAccountsUpdate,
        kAccountsLikes,
    };

    enum Filter : u32 {
        kSexEq = 1 << 0,

        kEmailDomain = 1 << 1,
        kEmailLt     = 1 << 2,
        kEmailGt     = 1 << 3,

        kStatusEq  = 1 << 4,
        kStatusNeq = 1 << 5,

        kFnameEq   = 1 << 6,
        kFnameAny  = 1 << 7,
        kFnameNull = 1 << 8,

        kSnameEq     = 1 << 9,
        kSnameStarts = 1 << 10,
        kSnameNull   = 1 << 11,

        kPhoneCode = 1 << 12,
        kPhoneNull = 1 << 13,

        kCountryEq   = 1 << 14,
        kCountryNull = 1 << 15,

        kCityEq   = 1 << 16,
        kCityAny  = 1 << 17,
        kCityNull = 1 << 18,

        kBirthLt   = 1 << 19,
        kBirthGt   = 1 << 20,
        kBirthYear = 1 << 21,

        kInterestsContains = 1 << 22,
        kInterestsAny      = 1 << 23,

        kLikesContains = 1 << 24,

        kPremiumNow  = 1 << 25,
        kPremiumNull = 1 << 26,
    };

    struct FilterParams {
        StringRef email, fname, sname, country, city, interests, likes;
        Timestamp birth;
        u16       phone;
        u8        limit;
        u8        sex : 1;
        u8        status : 2;
        u8        premium : 1;
    };

    enum Basic : u32 {
        kSex       = 1 << 0,
        kStatus    = 1 << 1,
        kCountry   = 1 << 2,
        kCity      = 1 << 3,
        kBirth     = 1 << 4,
        kInterests = 1 << 5,
        kLikes     = 1 << 6,
        kJoined    = 1 << 7,
    };

    enum Order : u8 {
        kAsc = 0,
        kDesc,
    };

    struct BasicParams {
        StringRef country, city, interests;
        Timestamp birth, joined;
        u32       entity_id;
        u32       likes;
        u16       keys;
        u8        limit;
        u8        order : 1;
        u8        sex : 1;
        u8        status : 2;
    };

    enum Key : u8 {
        kSexKey = 0,
        kStatusKey,
        kCountryKey,
        kCityKey,
        kInterestsKey,
    };

    u32  req_id;
    u32  mask;
    Type type : 8;

    union {
        FilterParams filter;
        BasicParams  basic;
    } query;

    char *string_data;

    inline std::string_view getView(const StringRef &ref) const { return std::string_view(string_data + ref.offset, ref.size); }
    inline std::string      getString(const StringRef &ref) const { return std::string(string_data + ref.offset, ref.size); }

    inline void setSex(Filter param, Sex sex) {
        mask |= param;
        query.filter.sex = sex;
    }

    inline void setStatus(Filter param, Status st) {
        mask |= param;
        query.filter.status = st;
    }

    inline void setEmail(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.email = val;
    }

    inline void setFname(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.fname = val;
    }

    inline void setSname(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.sname = val;
    }

    inline void setPhone(Filter param, u16 val) {
        mask |= param;
        query.filter.phone = val;
    }

    inline void setCountry(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.country = val;
    }

    inline void setCity(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.city = val;
    }

    inline void setBirth(Filter param, Timestamp val) {
        mask |= param;
        query.filter.birth = val;
    }

    inline void setInterests(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.interests = val;
    }

    inline void setLikes(Filter param, const StringRef &val) {
        mask |= param;
        query.filter.likes = val;
    }

    inline void setPremium(Filter param, bool val) {
        mask |= param;
        query.filter.premium = val;
    }

    Request() {
        string_data = reinterpret_cast<char *>(::aligned_alloc(8192, 8192));
        assert(string_data);
    }

    ~Request() { ::free(string_data); }
};

}  // namespace hlcup
