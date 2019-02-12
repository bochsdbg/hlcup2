//#include "syscall.hpp"
//#include <iostream>
#include "socket.hpp"

#include <arpa/inet.h>

#include <cstring>

#include <cstdio>
#include <iostream>

//#include <boost/spirit/include/qi.hpp>
//#include <boost/spirit/include/qi_parse.hpp>
//#include <boost/spirit/include/qi_uint.hpp>

// namespace qi = boost::spirit::qi;

// struct A {
//    int a, b;
//};

// template <typename IteratorT, typename SkipperT>
// struct MyGrammar : public qi::grammar<IteratorT, A, SkipperT> {
//    MyGrammar()
//        : MyGrammar::base_type(objectRule, "Object"){auto abc_rule = qi::lit("abc: ") >> objectRule = qi::lit("{") >> }

//          qi::rule<IteratorT, A, SkipperT> objectRule;
//};

#include "AccountParser.hpp"

#include <codecvt>
#include <locale>

#include "core/SmallString.hpp"
#include "miniz.h"

#include <emmintrin.h>
#include <chrono>
#include <map>
#include <unordered_map>

#include "Time.hpp"

#include "HttpParser.hpp"
#include "ParseUtils.hpp"

#include "fmt/format.hpp"

template <class T>
void print_stats(const std::string &name, const std::map<T, size_t> &st) {
    std::cout << "!!! " << name << " count: " << st.size() << " ----------------------- " << std::endl;
    for (const auto &it : st) { std::cout << it.first << ": " << it.second << std::endl; }
}

#define BENCH_ONLY 1

using namespace ef;

int main() {
    //    {
    //        hlcup::Request req;
    //        std::string    buf(
    //            "GET /accounts/123/suggest/?request_id=456&country=%2f%31d&limit=63 HTTP/1.1\r\n"
    //            "content-length: 457\r\n"
    //            "\r\n");
    //        hlcup::HttpParser parser;
    //        parser.reset();
    //        parser.parse(buf.data(), buf.data() + buf.size(), req);

    //        fmt::print("clen: {}\n", parser.content_length);
    //        fmt::print("country: {}\n", req.getString(req.query.basic.country));
    //        fmt::print("Req id: {}, type: {}, entity_id: {}\n", req.req_id, req.type, req.query.basic.entity_id);
    //        fmt::print("Limit: {}\n", req.query.basic.limit);

    //        return 0;
    //    }

    //    hlcup::HttpParser p;
    //    std::string       req_buf("POST /accounts/123");
    //    hlcup::Request    req;
    //    char *            ptr = req_buf.data();
    //    p.parse(ptr, req_buf.data() + req_buf.size(), req);
    //    return 0;
    //    //    fmt::print("size: {}\n", sizeof(hlcup::Request::FilterParams));
    //    //    return 0;

    //    int fds[2];

    //    [[maybe_unused]] int rs = platform::socketpair(PF_LOCAL, SOCK_STREAM, 0, fds);

    //    msghdr msg;
    //    iovec  iov[5];

    //    iov[0].iov_base = (void *)"hello";
    //    iov[0].iov_len  = 5;

    //    msg.msg_name       = nullptr;
    //    msg.msg_iovlen     = 1;
    //    msg.msg_control    = nullptr;
    //    msg.msg_controllen = 0;
    //    msg.msg_namelen    = 0;

    //    msg.msg_iov = iov;

    //    long err;
    //    err = platform::sendmsg(fds[0], &msg, 0);
    //    fmt::print("sendmsg(): {}\n", err);

    //    if (err < 0) return 0;

    //    char mybuf[512];
    //    err = platform::recv(fds[1], mybuf, 512, 0);
    //    fmt::print("recv(): {}\n", err);

    //    if (err > 0) { fmt::print("buf: {:.{}}\n", mybuf, err); }
    //    return 0;
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    hlcup::Account       acc;
    hlcup::AccountParser parser;

    mz_zip_reader_init_file(&zip, "/home/me/prj/hlcup2/rating/data/data.zip", 0);
    mz_uint           num_files = mz_zip_reader_get_num_files(&zip);
    std::vector<char> buf;

    auto start_time = std::chrono::steady_clock::now();

    int cnt = 0;

    std::map<std::string, size_t> fnames, snames, phone_codes, email_domains, countries, cities, phone_first_chars, interests;
    std::map<size_t, size_t>      likes_counts, interests_counts, email_logins_lengths;
    std::map<char, size_t>        emails_chars;
    std::map<int, size_t>         birth_years, joined_years;

    //    hlcup::Timestamp min_joined = std::numeric_limits<hlcup::Timestamp>::max(), max_joined = std::numeric_limits<hlcup::Timestamp>::min(),
    //                     min_birth = std::numeric_limits<hlcup::Timestamp>::max(), max_birth = std::numeric_limits<hlcup::Timestamp>::min();

    for (mz_uint i = 0; i < num_files; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            fprintf(stderr, "mz_zip_reader_file_stat() failed: %s\n", mz_error(zip.m_last_error));
            break;
        }

        buf.resize(st.m_uncomp_size);
        if (!mz_zip_reader_extract_to_mem(&zip, i, buf.data(), buf.size(), 0)) {
            fprintf(stderr, "mz_zip_reader_extract_to_mem() failed: %s\n", mz_error(zip.m_last_error));
            break;
        }

        const char *p  = buf.data();
        const char *pe = buf.data() + buf.size();
        while (p < pe && *p != '[') ++p;
        ++p;

        //        p = pe;
        while (p < pe) {
            if (!parser.parse(p, pe, acc)) break;
            ++cnt;
#if !BENCH_ONLY
            if (acc.birth != hlcup::kInvalidTimestamp) {
                hlcup::Time b(acc.birth);
                birth_years[b.year]++;
                //                std::cout << acc.birth << std::endl;
                //                std::cout << t.mday << "." << t.mon << "." << t.year << " " << t.hour << ":" << t.min << ":" << t.sec << std::endl;
            }

            if (acc.joined != hlcup::kInvalidTimestamp) {
                hlcup::Time j(acc.joined);
                joined_years[j.year]++;
            }
            //            for (size_t i = 0; i < acc.getInterestsCount(); i++) { interests[acc.getString(acc.getInterest(i))]++; }

            //            if (acc.birth != hlcup::kInvalidTimestamp) min_birth = std::min(min_birth, acc.birth);
            //            if (acc.joined != hlcup::kInvalidTimestamp) min_joined = std::min(min_joined, acc.joined);
            //            if (acc.birth != hlcup::kInvalidTimestamp) max_birth = std::max(max_birth, acc.birth);
            //            if (acc.joined != hlcup::kInvalidTimestamp) max_joined = std::max(max_joined, acc.joined);

            //            if (acc.fname.offset != hlcup::Account::kInvalidOffset) { fnames[acc.getString(acc.fname)]++; }
            //            if (acc.sname.offset != hlcup::Account::kInvalidOffset) { snames[acc.getString(acc.sname)]++; }
            //            if (acc.city.offset != hlcup::Account::kInvalidOffset) { cities[acc.getString(acc.city)]++; }
            if (acc.country.offset != hlcup::Account::kInvalidOffset) { countries[acc.getString(acc.country)]++; }
            //            if (acc.email.offset != hlcup::Account::kInvalidOffset) {
            //                auto email = acc.getString(acc.email);
            //                auto pos   = email.find_last_of('@');
            //                for (size_t i = 0; i < pos; i++) {
            //                    if (email[i] < 'a' || email[i] > 'z') {
            //                        ccc++;
            //                        std::cout << email << std::endl;
            //                        //                        if (ccc > 50) return 0;
            //                    }
            //                    emails_chars[email[i]]++;
            //                }
            //                email_logins_lengths[pos]++;

            //                if (pos != std::string_view::npos) { email_domains[email.substr(pos)]++; }
            //            }
            //            if (acc.phone.offset != hlcup::Account::kInvalidOffset) {
            //                auto phone = acc.getString(acc.phone);
            //                phone_first_chars[phone.substr(0, 1)]++;
            //                phone_codes[phone.substr(1, 5)]++;
            //            }

            if (acc.getInterestsCount() > 90) {
                std::cout << st.m_filename << std::endl;
                std::cout << acc.id << std::endl;
                break;
            }
            interests_counts[acc.getInterestsCount()]++;
            //            likes_counts[acc.likes.size()]++;
#endif
            acc.clear();
            while (p < pe && (*p != '{')) ++p;
        }

        //            //            std::cout << std::string_view(p, 50) << std::endl;
        //        }

        //        std::cout << cnt << std::endl;

        //        std::cout << st.m_filename << " " << acc.id << std::endl;
        //        break;
        //        std::cout << std::string_view(buf.data(), 50) << std::endl;
        //        std::cout << st.m_filename << " " << st.m_comp_size << " " << st.m_uncomp_size << std::endl;
    }
    mz_zip_end(&zip);

#if !BENCH_ONLY
    //    print_stats("fnames", fnames);
    //    print_stats("snames", snames);
    //    print_stats("cities", cities);
    print_stats("countries", countries);
    //    print_stats("email_domains", email_domains);
    //    print_stats("phone_codes", phone_codes);
    //    print_stats("phone_first_chars", phone_first_chars);
    //    print_stats("likes_counts", likes_counts);
    print_stats("interests_counts", interests_counts);
    //    print_stats("email_logins_lengths", email_logins_lengths);
    //    print_stats("emails_chars", emails_chars);
    //    print_stats("interests", interests);
    //    std::cout << "Joined: " << min_joined << " " << max_joined << std::endl;
    //    std::cout << "Birth: " << min_birth << " " << max_birth << std::endl;
    print_stats("joined_years", joined_years);
    print_stats("birth_years", birth_years);
#endif

    std::cout << cnt << std::endl;
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() << std::endl;

    return 0;
}
