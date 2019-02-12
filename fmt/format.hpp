/*
 Formatting library for C++

 Copyright (c) 2012 - present, Victor Zverovich
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FMT_FORMAT_H_
#define FMT_FORMAT_H_

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

#ifdef __clang__
#define FMT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
#define FMT_CLANG_VERSION 0
#endif

#ifdef __INTEL_COMPILER
#define FMT_ICC_VERSION __INTEL_COMPILER
#elif defined(__ICL)
#define FMT_ICC_VERSION __ICL
#else
#define FMT_ICC_VERSION 0
#endif

#ifdef __NVCC__
#define FMT_CUDA_VERSION (__CUDACC_VER_MAJOR__ * 100 + __CUDACC_VER_MINOR__)
#else
#define FMT_CUDA_VERSION 0
#endif

// Formatting library for C++ - the core API
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_CORE_H_
#define FMT_CORE_H_

#include <cassert>
#include <cstdio>  // std::FILE
#include <cstring>
#include <iterator>
#include <string>
#include <type_traits>

// The fmt library version in the form major * 10000 + minor * 100 + patch.
#define FMT_VERSION 50301

#ifdef __has_feature
#define FMT_HAS_FEATURE(x) __has_feature(x)
#else
#define FMT_HAS_FEATURE(x) 0
#endif

#if defined(__has_include) && !defined(__INTELLISENSE__) && !(defined(__INTEL_COMPILER) && __INTEL_COMPILER < 1600)
#define FMT_HAS_INCLUDE(x) __has_include(x)
#else
#define FMT_HAS_INCLUDE(x) 0
#endif

#ifdef __has_cpp_attribute
#define FMT_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define FMT_HAS_CPP_ATTRIBUTE(x) 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define FMT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define FMT_GCC_VERSION 0
#endif

#if __cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__)
#define FMT_HAS_GXX_CXX11 FMT_GCC_VERSION
#else
#define FMT_HAS_GXX_CXX11 0
#endif

#ifdef _MSC_VER
#define FMT_MSC_VER _MSC_VER
#else
#define FMT_MSC_VER 0
#endif

#ifndef FMT_BEGIN_NAMESPACE
#if FMT_HAS_FEATURE(cxx_inline_namespaces) || FMT_GCC_VERSION >= 404 || FMT_MSC_VER >= 1900
#define FMT_INLINE_NAMESPACE inline namespace
#define FMT_END_NAMESPACE \
    }                     \
    }
#else
#define FMT_INLINE_NAMESPACE namespace
#define FMT_END_NAMESPACE \
    }                     \
    using namespace v5;   \
    }
#endif
#define FMT_BEGIN_NAMESPACE \
    namespace fmt {         \
    FMT_INLINE_NAMESPACE v5 {
#endif

#ifndef FMT_API
#define FMT_API
#endif

#ifndef FMT_ASSERT
#define FMT_ASSERT(condition, message) assert((condition) && message)
#endif

#include <functional>
#include <string_view>

FMT_BEGIN_NAMESPACE

namespace internal {

// Casts nonnegative integer to unsigned.
template <typename Int>
constexpr typename std::make_unsigned<Int>::type to_unsigned(Int value) {
    FMT_ASSERT(value >= 0, "negative value");
    return static_cast<typename std::make_unsigned<Int>::type>(value);
}

/** A contiguous memory buffer with an optional growing ability. */
template <typename T>
class basic_buffer {
private:
    basic_buffer(const basic_buffer &) = delete;
    void operator=(const basic_buffer &) = delete;

    T *         ptr_;
    std::size_t size_;
    std::size_t capacity_;

protected:
    // Don't initialize ptr_ since it is not accessed to save a few cycles.
    basic_buffer(std::size_t sz) noexcept : size_(sz), capacity_(sz) {}

    basic_buffer(T *p = nullptr, std::size_t sz = 0, std::size_t cap = 0) noexcept : ptr_(p), size_(sz), capacity_(cap) {}

    /** Sets the buffer data and capacity. */
    void set(T *buf_data, std::size_t buf_capacity) noexcept {
        ptr_      = buf_data;
        capacity_ = buf_capacity;
    }

    /** Increases the buffer capacity to hold at least *capacity* elements. */
    virtual void grow(std::size_t capacity) = 0;

public:
    typedef T        value_type;
    typedef const T &const_reference;

    virtual ~basic_buffer() {}

    T *begin() noexcept { return ptr_; }
    T *end() noexcept { return ptr_ + size_; }

    /** Returns the size of this buffer. */
    std::size_t size() const noexcept { return size_; }

    /** Returns the capacity of this buffer. */
    std::size_t capacity() const noexcept { return capacity_; }

    /** Returns a pointer to the buffer data. */
    T *data() noexcept { return ptr_; }

    /** Returns a pointer to the buffer data. */
    const T *data() const noexcept { return ptr_; }

    /**
      Resizes the buffer. If T is a POD type new elements may not be initialized.
     */
    void resize(std::size_t new_size) {
        reserve(new_size);
        size_ = new_size;
    }

    /** Clears this buffer. */
    void clear() { size_ = 0; }

    /** Reserves space to store at least *capacity* elements. */
    void reserve(std::size_t new_capacity) {
        if (new_capacity > capacity_) grow(new_capacity);
    }

    void push_back(const T &value) {
        reserve(size_ + 1);
        ptr_[size_++] = value;
    }

    /** Appends data to the end of the buffer. */
    template <typename U>
    void append(const U *begin, const U *end);

    T &      operator[](std::size_t index) { return ptr_[index]; }
    const T &operator[](std::size_t index) const { return ptr_[index]; }
};

typedef basic_buffer<char>    buffer;
typedef basic_buffer<wchar_t> wbuffer;

// A container-backed buffer.
template <typename Container>
class container_buffer : public basic_buffer<typename Container::value_type> {
private:
    Container &container_;

protected:
    void grow(std::size_t capacity) override {
        container_.resize(capacity);
        this->set(&container_[0], capacity);
    }

public:
    explicit container_buffer(Container &c) : basic_buffer<typename Container::value_type>(c.size()), container_(c) {}
};

// Extracts a reference to the container from back_insert_iterator.
template <typename Container>
inline Container &get_container(std::back_insert_iterator<Container> it) {
    typedef std::back_insert_iterator<Container> bi_iterator;
    struct accessor : bi_iterator {
        accessor(bi_iterator iter) : bi_iterator(iter) {}
        using bi_iterator::container;
    };
    return *accessor(it).container;
}

struct error_handler {
    constexpr error_handler() {}
    constexpr error_handler(const error_handler &) {}

    // This function is intentionally not constexpr to give a compile-time error.
    [[noreturn]] FMT_API void on_error(const char *message);
};

template <typename T>
struct no_formatter_error : std::false_type {};
}  // namespace internal

#if FMT_GCC_VERSION && FMT_GCC_VERSION < 405
template <typename... T>
struct is_constructible : std::false_type {};
#else
template <typename... T>
struct is_constructible : std::is_constructible<T...> {};
#endif

/**
  An implementation of ``std::basic_string_view`` for pre-C++17. It provides a
  subset of the API. ``fmt::basic_string_view`` is used for format strings even
  if ``std::string_view`` is available to prevent issues when a library is
  compiled with a different ``-std`` option than the client code (which is not
  recommended).
 */
template <typename Char>
class basic_string_view {
private:
    const Char *data_;
    size_t      size_;

public:
    typedef Char        char_type;
    typedef const Char *iterator;

    constexpr basic_string_view() noexcept : data_(nullptr), size_(0) {}

    /** Constructs a string reference object from a C string and a size. */
    constexpr basic_string_view(const Char *s, size_t count) noexcept : data_(s), size_(count) {}

    /**
      \rst
      Constructs a string reference object from a C string computing
      the size with ``std::char_traits<Char>::length``.
      \endrst
     */
    basic_string_view(const Char *s) : data_(s), size_(std::char_traits<Char>::length(s)) {}

    /** Constructs a string reference from a ``std::basic_string`` object. */
    template <typename Alloc>
    constexpr basic_string_view(const std::basic_string<Char, Alloc> &s) noexcept : data_(s.data()), size_(s.size()) {}

#ifdef FMT_STRING_VIEW
    constexpr basic_string_view(FMT_STRING_VIEW<Char> s) noexcept : data_(s.data()), size_(s.size()) {}
#endif

    /** Returns a pointer to the string data. */
    constexpr const Char *data() const { return data_; }

    /** Returns the string size. */
    constexpr size_t size() const { return size_; }

    constexpr iterator begin() const { return data_; }
    constexpr iterator end() const { return data_ + size_; }

    constexpr void remove_prefix(size_t n) {
        data_ += n;
        size_ -= n;
    }

    // Lexicographically compare this string reference to other.
    int compare(basic_string_view other) const {
        size_t str_size = size_ < other.size_ ? size_ : other.size_;
        int    result   = std::char_traits<Char>::compare(data_, other.data_, str_size);
        if (result == 0) result = size_ == other.size_ ? 0 : (size_ < other.size_ ? -1 : 1);
        return result;
    }

    friend bool operator==(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) == 0; }
    friend bool operator!=(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) != 0; }
    friend bool operator<(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) < 0; }
    friend bool operator<=(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) <= 0; }
    friend bool operator>(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) > 0; }
    friend bool operator>=(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) >= 0; }
};

typedef basic_string_view<char>    string_view;
typedef basic_string_view<wchar_t> wstring_view;

/**
  \rst
  The function ``to_string_view`` adapts non-intrusively any kind of string or
  string-like type if the user provides a (possibly templated) overload of
  ``to_string_view`` which takes an instance of the string class
  ``StringType<Char>`` and returns a ``fmt::basic_string_view<Char>``.
  The conversion function must live in the very same namespace as
  ``StringType<Char>`` to be picked up by ADL. Non-templated string types
  like f.e. QString must return a ``basic_string_view`` with a fixed matching
  char type.

  **Example**::

    namespace my_ns {
    inline string_view to_string_view(const my_string &s) {
      return {s.data(), s.length()};
    }
    }

    std::string message = fmt::format(my_string("The answer is {}"), 42);
  \endrst
 */
template <typename Char>
inline basic_string_view<Char> to_string_view(basic_string_view<Char> s) {
    return s;
}

template <typename Char>
inline basic_string_view<Char> to_string_view(const std::basic_string<Char> &s) {
    return s;
}

template <typename Char>
inline basic_string_view<Char> to_string_view(const Char *s) {
    return s;
}

#ifdef FMT_STRING_VIEW
template <typename Char>
inline basic_string_view<Char> to_string_view(FMT_STRING_VIEW<Char> s) {
    return s;
}
#endif

// A base class for compile-time strings. It is defined in the fmt namespace to
// make formatting functions visible via ADL, e.g. format(fmt("{}"), 42).
struct compile_string {};

template <typename S>
struct is_compile_string : std::is_base_of<compile_string, S> {};

template <typename S, typename Enable = typename std::enable_if<is_compile_string<S>::value>::type>
constexpr basic_string_view<typename S::char_type> to_string_view(const S &s) {
    return s;
}

template <typename Context>
class basic_format_arg;

template <typename Context>
class basic_format_args;

// A formatter for objects of type T.
template <typename T, typename Char = char, typename Enable = void>
struct formatter {
    static_assert(internal::no_formatter_error<T>::value,
                  "don't know how to format the type, include fmt/ostream.h if it provides "
                  "an operator<< that should be used");

    // The following functions are not defined intentionally.
    template <typename ParseContext>
    typename ParseContext::iterator parse(ParseContext &);
    template <typename FormatContext>
    auto format(const T &val, FormatContext &ctx) -> decltype(ctx.out());
};

template <typename T, typename Char, typename Enable = void>
struct convert_to_int : std::integral_constant<bool, !std::is_arithmetic<T>::value && std::is_convertible<T, int>::value> {};

namespace internal {

struct dummy_string_view {
    typedef void char_type;
};
dummy_string_view to_string_view(...);
using fmt::v5::to_string_view;

// Specifies whether S is a string type convertible to fmt::basic_string_view.
template <typename S>
struct is_string : std::integral_constant<bool, !std::is_same<dummy_string_view, decltype(to_string_view(std::declval<S>()))>::value> {};

template <typename S>
struct char_t {
    typedef decltype(to_string_view(std::declval<S>())) result;
    typedef typename result::char_type                  type;
};

template <typename Char>
struct named_arg_base;

template <typename T, typename Char>
struct named_arg;

enum type {
    none_type,
    named_arg_type,
    // Integer types should go first,
    int_type,
    uint_type,
    long_long_type,
    ulong_long_type,
    bool_type,
    char_type,
    last_integer_type = char_type,
    // followed by floating-point types.
    double_type,
    long_double_type,
    last_numeric_type = long_double_type,
    cstring_type,
    string_type,
    pointer_type,
    custom_type
};

constexpr bool is_integral(type t) {
    FMT_ASSERT(t != internal::named_arg_type, "invalid argument type");
    return t > internal::none_type && t <= internal::last_integer_type;
}

constexpr bool is_arithmetic(type t) {
    FMT_ASSERT(t != internal::named_arg_type, "invalid argument type");
    return t > internal::none_type && t <= internal::last_numeric_type;
}

template <typename Char>
struct string_value {
    const Char *value;
    std::size_t size;
};

template <typename Context>
struct custom_value {
    const void *value;
    void (*format)(const void *arg, Context &ctx);
};

// A formatting argument value.
template <typename Context>
class value {
public:
    typedef typename Context::char_type char_type;

    union {
        int                         int_value;
        unsigned                    uint_value;
        long long                   long_long_value;
        unsigned long long          ulong_long_value;
        double                      double_value;
        long double                 long_double_value;
        const void *                pointer;
        string_value<char_type>     string;
        string_value<signed char>   sstring;
        string_value<unsigned char> ustring;
        custom_value<Context>       custom;
    };

    constexpr value(int val = 0) : int_value(val) {}
    value(unsigned val) { uint_value = val; }
    value(long long val) { long_long_value = val; }
    value(unsigned long long val) { ulong_long_value = val; }
    value(double val) { double_value = val; }
    value(long double val) { long_double_value = val; }
    value(const char_type *val) { string.value = val; }
    value(const signed char *val) {
        static_assert(std::is_same<char, char_type>::value, "incompatible string types");
        sstring.value = val;
    }
    value(const unsigned char *val) {
        static_assert(std::is_same<char, char_type>::value, "incompatible string types");
        ustring.value = val;
    }
    value(basic_string_view<char_type> val) {
        string.value = val.data();
        string.size  = val.size();
    }
    value(const void *val) { pointer = val; }

    template <typename T>
    explicit value(const T &val) {
        custom.value  = &val;
        custom.format = &format_custom_arg<T>;
    }

    const named_arg_base<char_type> &as_named_arg() { return *static_cast<const named_arg_base<char_type> *>(pointer); }

private:
    // Formats an argument of a custom type, such as a user-defined class.
    template <typename T>
    static void format_custom_arg(const void *arg, Context &ctx) {
        // Get the formatter type through the context to allow different contexts
        // have different extension points, e.g. `formatter<T>` for `format` and
        // `printf_formatter<T>` for `printf`.
        typename Context::template formatter_type<T>::type f;
        auto &&                                            parse_ctx = ctx.parse_context();
        parse_ctx.advance_to(f.parse(parse_ctx));
        ctx.advance_to(f.format(*static_cast<const T *>(arg), ctx));
    }
};

// Value initializer used to delay conversion to value and reduce memory churn.
template <typename Context, typename T, type TYPE>
struct init {
    T                 val;
    static const type type_tag = TYPE;

    constexpr init(const T &v) : val(v) {}
    constexpr operator value<Context>() const { return value<Context>(val); }
};

template <typename Context, typename T>
constexpr basic_format_arg<Context> make_arg(const T &value);

#define FMT_MAKE_VALUE(TAG, ArgType, ValueType)                 \
    template <typename C>                                       \
    constexpr init<C, ValueType, TAG> make_value(ArgType val) { \
        return static_cast<ValueType>(val);                     \
    }

#define FMT_MAKE_VALUE_SAME(TAG, Type)                  \
    template <typename C>                               \
    constexpr init<C, Type, TAG> make_value(Type val) { \
        return val;                                     \
    }

FMT_MAKE_VALUE(bool_type, bool, int)
FMT_MAKE_VALUE(int_type, short, int)
FMT_MAKE_VALUE(uint_type, unsigned short, unsigned)
FMT_MAKE_VALUE_SAME(int_type, int)
FMT_MAKE_VALUE_SAME(uint_type, unsigned)

// To minimize the number of types we need to deal with, long is translated
// either to int or to long long depending on its size.
typedef std::conditional<sizeof(long) == sizeof(int), int, long long>::type long_type;
FMT_MAKE_VALUE((sizeof(long) == sizeof(int) ? int_type : long_long_type), long, long_type)
typedef std::conditional<sizeof(unsigned long) == sizeof(unsigned), unsigned, unsigned long long>::type ulong_type;
FMT_MAKE_VALUE((sizeof(unsigned long) == sizeof(unsigned) ? uint_type : ulong_long_type), unsigned long, ulong_type)

FMT_MAKE_VALUE_SAME(long_long_type, long long)
FMT_MAKE_VALUE_SAME(ulong_long_type, unsigned long long)
FMT_MAKE_VALUE(int_type, signed char, int)
FMT_MAKE_VALUE(uint_type, unsigned char, unsigned)

// This doesn't use FMT_MAKE_VALUE because of ambiguity in gcc 4.4.
template <typename C, typename Char>
constexpr typename std::enable_if<std::is_same<typename C::char_type, Char>::value, init<C, int, char_type>>::type make_value(Char val) {
    return val;
}

template <typename C>
constexpr typename std::enable_if<!std::is_same<typename C::char_type, char>::value, init<C, int, char_type>>::type make_value(char val) {
    return val;
}

FMT_MAKE_VALUE(double_type, float, double)
FMT_MAKE_VALUE_SAME(double_type, double)
FMT_MAKE_VALUE_SAME(long_double_type, long double)

// Formatting of wide strings into a narrow buffer and multibyte strings
// into a wide buffer is disallowed (https://github.com/fmtlib/fmt/pull/606).
FMT_MAKE_VALUE(cstring_type, typename C::char_type *, const typename C::char_type *)
FMT_MAKE_VALUE(cstring_type, const typename C::char_type *, const typename C::char_type *)

FMT_MAKE_VALUE(cstring_type, signed char *, const signed char *)
FMT_MAKE_VALUE_SAME(cstring_type, const signed char *)
FMT_MAKE_VALUE(cstring_type, unsigned char *, const unsigned char *)
FMT_MAKE_VALUE_SAME(cstring_type, const unsigned char *)
FMT_MAKE_VALUE_SAME(string_type, basic_string_view<typename C::char_type>)
FMT_MAKE_VALUE(string_type, typename basic_string_view<typename C::char_type>::type, basic_string_view<typename C::char_type>)
FMT_MAKE_VALUE(string_type, const std::basic_string<typename C::char_type> &, basic_string_view<typename C::char_type>)
FMT_MAKE_VALUE(pointer_type, void *, const void *)
FMT_MAKE_VALUE_SAME(pointer_type, const void *)

FMT_MAKE_VALUE(pointer_type, std::nullptr_t, const void *)

// Formatting of arbitrary pointers is disallowed. If you want to output a
// pointer cast it to "void *" or "const void *". In particular, this forbids
// formatting of "[const] volatile char *" which is printed as bool by
// iostreams.
template <typename C, typename T>
typename std::enable_if<!std::is_same<T, typename C::char_type>::value>::type make_value(const T *) {
    static_assert(!sizeof(T), "formatting of non-void pointers is disallowed");
}

template <typename C, typename T>
inline typename std::enable_if<std::is_enum<T>::value && convert_to_int<T, typename C::char_type>::value, init<C, int, int_type>>::type make_value(
    const T &val) {
    return static_cast<int>(val);
}

template <typename C, typename T, typename Char = typename C::char_type>
inline typename std::enable_if<is_constructible<basic_string_view<Char>, T>::value && !internal::is_string<T>::value,
                               init<C, basic_string_view<Char>, string_type>>::type
make_value(const T &val) {
    return basic_string_view<Char>(val);
}

template <typename C, typename T, typename Char = typename C::char_type>
inline typename std::enable_if<!convert_to_int<T, Char>::value && !std::is_same<T, Char>::value && !std::is_convertible<T, basic_string_view<Char>>::value
                                   && !is_constructible<basic_string_view<Char>, T>::value && !internal::is_string<T>::value,
                               // Implicit conversion to std::string is not handled here because it's
                               // unsafe: https://github.com/fmtlib/fmt/issues/729
                               init<C, const T &, custom_type>>::type
make_value(const T &val) {
    return val;
}

template <typename C, typename T>
init<C, const void *, named_arg_type> make_value(const named_arg<T, typename C::char_type> &val) {
    basic_format_arg<C> arg = make_arg<C>(val.value);
    std::memcpy(val.data, &arg, sizeof(arg));
    return static_cast<const void *>(&val);
}

template <typename C, typename S>
constexpr typename std::enable_if<internal::is_string<S>::value, init<C, basic_string_view<typename C::char_type>, string_type>>::type make_value(
    const S &val) {
    // Handle adapted strings.
    static_assert(std::is_same<typename C::char_type, typename internal::char_t<S>::type>::value, "mismatch between char-types of context and argument");
    return to_string_view(val);
}

// Maximum number of arguments with packed types.
enum { max_packed_args = 15 };
enum : unsigned long long { is_unpacked_bit = 1ull << 63 };

template <typename Context>
class arg_map;
}  // namespace internal

// A formatting argument. It is a trivially copyable/constructible type to
// allow storage in basic_memory_buffer.
template <typename Context>
class basic_format_arg {
private:
    internal::value<Context> value_;
    internal::type           type_;

    template <typename ContextType, typename T>
    friend constexpr basic_format_arg<ContextType> internal::make_arg(const T &value);

    template <typename Visitor, typename Ctx>
    friend constexpr typename std::result_of<Visitor(int)>::type visit_format_arg(Visitor &&vis, const basic_format_arg<Ctx> &arg);

    friend class basic_format_args<Context>;
    friend class internal::arg_map<Context>;

    typedef typename Context::char_type char_type;

public:
    class handle {
    public:
        explicit handle(internal::custom_value<Context> custom) : custom_(custom) {}

        void format(Context &ctx) const { custom_.format(custom_.value, ctx); }

    private:
        internal::custom_value<Context> custom_;
    };

    constexpr basic_format_arg() : type_(internal::none_type) {}

    explicit operator bool() const noexcept { return type_ != internal::none_type; }

    internal::type type() const { return type_; }

    bool is_integral() const { return internal::is_integral(type_); }
    bool is_arithmetic() const { return internal::is_arithmetic(type_); }
};

struct monostate {};

/**
  \rst
  Visits an argument dispatching to the appropriate visit method based on
  the argument type. For example, if the argument type is ``double`` then
  ``vis(value)`` will be called with the value of type ``double``.
  \endrst
 */
template <typename Visitor, typename Context>
constexpr typename std::result_of<Visitor(int)>::type visit_format_arg(Visitor &&vis, const basic_format_arg<Context> &arg) {
    typedef typename Context::char_type char_type;
    switch (arg.type_) {
    case internal::none_type: break;
    case internal::named_arg_type: FMT_ASSERT(false, "invalid argument type"); break;
    case internal::int_type: return vis(arg.value_.int_value);
    case internal::uint_type: return vis(arg.value_.uint_value);
    case internal::long_long_type: return vis(arg.value_.long_long_value);
    case internal::ulong_long_type: return vis(arg.value_.ulong_long_value);
    case internal::bool_type: return vis(arg.value_.int_value != 0);
    case internal::char_type: return vis(static_cast<char_type>(arg.value_.int_value));
    case internal::double_type: return vis(arg.value_.double_value);
    case internal::long_double_type: return vis(arg.value_.long_double_value);
    case internal::cstring_type: return vis(arg.value_.string.value);
    case internal::string_type: return vis(basic_string_view<char_type>(arg.value_.string.value, arg.value_.string.size));
    case internal::pointer_type: return vis(arg.value_.pointer);
    case internal::custom_type: return vis(typename basic_format_arg<Context>::handle(arg.value_.custom));
    }
    return vis(monostate());
}

// DEPRECATED!
template <typename Visitor, typename Context>
constexpr typename std::result_of<Visitor(int)>::type visit(Visitor &&vis, const basic_format_arg<Context> &arg) {
    return visit_format_arg(std::forward<Visitor>(vis), arg);
}

// Parsing context consisting of a format string range being parsed and an
// argument counter for automatic indexing.
template <typename Char, typename ErrorHandler = internal::error_handler>
class basic_parse_context : private ErrorHandler {
private:
    basic_string_view<Char> format_str_;
    int                     next_arg_id_;

public:
    typedef Char                                       char_type;
    typedef typename basic_string_view<Char>::iterator iterator;

    explicit constexpr basic_parse_context(basic_string_view<Char> format_str, ErrorHandler eh = ErrorHandler())
        : ErrorHandler(eh), format_str_(format_str), next_arg_id_(0) {}

    // Returns an iterator to the beginning of the format string range being
    // parsed.
    constexpr iterator begin() const noexcept { return format_str_.begin(); }

    // Returns an iterator past the end of the format string range being parsed.
    constexpr iterator end() const noexcept { return format_str_.end(); }

    // Advances the begin iterator to ``it``.
    constexpr void advance_to(iterator it) { format_str_.remove_prefix(internal::to_unsigned(it - begin())); }

    // Returns the next argument index.
    constexpr unsigned next_arg_id();

    constexpr bool check_arg_id(unsigned) {
        if (next_arg_id_ > 0) {
            on_error("cannot switch from automatic to manual argument indexing");
            return false;
        }
        next_arg_id_ = -1;
        return true;
    }
    void check_arg_id(basic_string_view<Char>) {}

    constexpr void on_error(const char *message) { ErrorHandler::on_error(message); }

    constexpr ErrorHandler error_handler() const { return *this; }
};

typedef basic_parse_context<char>    format_parse_context;
typedef basic_parse_context<wchar_t> wformat_parse_context;

// DEPRECATED!
typedef basic_parse_context<char>    parse_context;
typedef basic_parse_context<wchar_t> wparse_context;

namespace internal {
// A map from argument names to their values for named arguments.
template <typename Context>
class arg_map {
private:
    arg_map(const arg_map &) = delete;
    void operator=(const arg_map &) = delete;

    typedef typename Context::char_type char_type;

    struct entry {
        basic_string_view<char_type> name;
        basic_format_arg<Context>    arg;
    };

    entry *  map_;
    unsigned size_;

    void push_back(value<Context> val) {
        const internal::named_arg_base<char_type> &named = val.as_named_arg();
        map_[size_]                                      = entry{named.name, named.template deserialize<Context>()};
        ++size_;
    }

public:
    arg_map() : map_(nullptr), size_(0) {}
    void init(const basic_format_args<Context> &args);
    ~arg_map() { delete[] map_; }

    basic_format_arg<Context> find(basic_string_view<char_type> name) const {
        // The list is unsorted, so just return the first matching name.
        for (entry *it = map_, *end = map_ + size_; it != end; ++it) {
            if (it->name == name) return it->arg;
        }
        return {};
    }
};

// A type-erased reference to an std::locale to avoid heavy <locale> include.
class locale_ref {
private:
    const void *locale_;  // A type-erased pointer to std::locale.
    friend class locale;

public:
    locale_ref() : locale_(nullptr) {}

    template <typename Locale>
    explicit locale_ref(const Locale &loc);

    template <typename Locale>
    Locale get() const;
};

template <typename OutputIt, typename Context, typename Char>
class context_base {
public:
    typedef OutputIt iterator;

private:
    basic_parse_context<Char>  parse_context_;
    iterator                   out_;
    basic_format_args<Context> args_;
    locale_ref                 loc_;

protected:
    typedef Char                      char_type;
    typedef basic_format_arg<Context> format_arg;

    context_base(OutputIt out, basic_string_view<char_type> format_str, basic_format_args<Context> ctx_args, locale_ref loc = locale_ref())
        : parse_context_(format_str), out_(out), args_(ctx_args), loc_(loc) {}

    // Returns the argument with specified index.
    format_arg do_get_arg(unsigned arg_id) {
        format_arg arg = args_.get(arg_id);
        if (!arg) parse_context_.on_error("argument index out of range");
        return arg;
    }

    // Checks if manual indexing is used and returns the argument with
    // specified index.
    format_arg get_arg(unsigned arg_id) { return this->parse_context().check_arg_id(arg_id) ? this->do_get_arg(arg_id) : format_arg(); }

public:
    basic_parse_context<char_type> &parse_context() { return parse_context_; }
    basic_format_args<Context>      args() const { return args_; }  // DEPRECATED!
    basic_format_arg<Context>       arg(unsigned id) const { return args_.get(id); }

    internal::error_handler error_handler() { return parse_context_.error_handler(); }

    void on_error(const char *message) { parse_context_.on_error(message); }

    // Returns an iterator to the beginning of the output range.
    iterator out() { return out_; }
    iterator begin() { return out_; }  // deprecated

    // Advances the begin iterator to ``it``.
    void advance_to(iterator it) { out_ = it; }

    locale_ref locale() { return loc_; }
};

template <typename Context, typename T>
struct get_type {
    typedef decltype(make_value<Context>(std::declval<typename std::decay<T>::type &>())) value_type;
    static const type                                                                     value = value_type::type_tag;
};

template <typename Context>
constexpr unsigned long long get_types() {
    return 0;
}

template <typename Context, typename Arg, typename... Args>
constexpr unsigned long long get_types() {
    return get_type<Context, Arg>::value | (get_types<Context, Args...>() << 4);
}

template <typename Context, typename T>
constexpr basic_format_arg<Context> make_arg(const T &value) {
    basic_format_arg<Context> arg;
    arg.type_  = get_type<Context, T>::value;
    arg.value_ = make_value<Context>(value);
    return arg;
}

template <bool IS_PACKED, typename Context, typename T>
inline typename std::enable_if<IS_PACKED, value<Context>>::type make_arg(const T &value) {
    return make_value<Context>(value);
}

template <bool IS_PACKED, typename Context, typename T>
inline typename std::enable_if<!IS_PACKED, basic_format_arg<Context>>::type make_arg(const T &value) {
    return make_arg<Context>(value);
}
}  // namespace internal

// Formatting context.
template <typename OutputIt, typename Char>
class basic_format_context : public internal::context_base<OutputIt, basic_format_context<OutputIt, Char>, Char> {
public:
    /** The character type for the output. */
    typedef Char char_type;

    // using formatter_type = formatter<T, char_type>;
    template <typename T>
    struct formatter_type {
        typedef formatter<T, char_type> type;
    };

private:
    internal::arg_map<basic_format_context> map_;

    basic_format_context(const basic_format_context &) = delete;
    void operator=(const basic_format_context &) = delete;

    typedef internal::context_base<OutputIt, basic_format_context, Char> base;
    typedef typename base::format_arg                                    format_arg;
    using base::get_arg;

public:
    using typename base::iterator;

    /**
     Constructs a ``basic_format_context`` object. References to the arguments are
     stored in the object so make sure they have appropriate lifetimes.
     */
    basic_format_context(OutputIt out, basic_string_view<char_type> format_str, basic_format_args<basic_format_context> ctx_args,
                         internal::locale_ref loc = internal::locale_ref())
        : base(out, format_str, ctx_args, loc) {}

    format_arg next_arg() { return this->do_get_arg(this->parse_context().next_arg_id()); }
    format_arg get_arg(unsigned arg_id) { return this->do_get_arg(arg_id); }

    // Checks if manual indexing is used and returns the argument with the
    // specified name.
    format_arg get_arg(basic_string_view<char_type> name);
};

template <typename Char>
struct buffer_context {
    typedef basic_format_context<std::back_insert_iterator<internal::basic_buffer<Char>>, Char> type;
};
typedef buffer_context<char>::type    format_context;
typedef buffer_context<wchar_t>::type wformat_context;

/**
  \rst
  An array of references to arguments. It can be implicitly converted into
  `~fmt::basic_format_args` for passing into type-erased formatting functions
  such as `~fmt::vformat`.
  \endrst
 */
template <typename Context, typename... Args>
class format_arg_store {
private:
    static const size_t NUM_ARGS = sizeof...(Args);

    // Packed is a macro on MinGW so use IS_PACKED instead.
    static const bool IS_PACKED = NUM_ARGS < internal::max_packed_args;

    typedef typename std::conditional<IS_PACKED, internal::value<Context>, basic_format_arg<Context>>::type value_type;

    // If the arguments are not packed, add one more element to mark the end.
    static const size_t DATA_SIZE = NUM_ARGS + (IS_PACKED && NUM_ARGS != 0 ? 0 : 1);
    value_type          data_[DATA_SIZE];

    friend class basic_format_args<Context>;

    static constexpr unsigned long long get_types() { return IS_PACKED ? internal::get_types<Context, Args...>() : internal::is_unpacked_bit | NUM_ARGS; }

public:
    static constexpr unsigned long long TYPES = get_types();
    format_arg_store(const Args &... args) : data_{internal::make_arg<IS_PACKED, Context>(args)...} {}
};

/**
  \rst
  Constructs an `~fmt::format_arg_store` object that contains references to
  arguments and can be implicitly converted to `~fmt::format_args`. `Context`
  can be omitted in which case it defaults to `~fmt::context`.
  \endrst
 */
template <typename Context = format_context, typename... Args>
inline format_arg_store<Context, Args...> make_format_args(const Args &... args) {
    return {args...};
}

/** Formatting arguments. */
template <typename Context>
class basic_format_args {
public:
    typedef unsigned                  size_type;
    typedef basic_format_arg<Context> format_arg;

private:
    // To reduce compiled code size per formatting function call, types of first
    // max_packed_args arguments are passed in the types_ field.
    unsigned long long types_;
    union {
        // If the number of arguments is less than max_packed_args, the argument
        // values are stored in values_, otherwise they are stored in args_.
        // This is done to reduce compiled code size as storing larger objects
        // may require more code (at least on x86-64) even if the same amount of
        // data is actually copied to stack. It saves ~10% on the bloat test.
        const internal::value<Context> *values_;
        const format_arg *              args_;
    };

    bool is_packed() const { return (types_ & internal::is_unpacked_bit) == 0; }

    typename internal::type type(unsigned index) const {
        unsigned shift = index * 4;
        return static_cast<typename internal::type>((types_ & (0xfull << shift)) >> shift);
    }

    friend class internal::arg_map<Context>;

    void set_data(const internal::value<Context> *values) { values_ = values; }
    void set_data(const format_arg *args) { args_ = args; }

    format_arg do_get(size_type index) const {
        format_arg arg;
        if (!is_packed()) {
            auto num_args = max_size();
            if (index < num_args) arg = args_[index];
            return arg;
        }
        if (index > internal::max_packed_args) return arg;
        arg.type_ = type(index);
        if (arg.type_ == internal::none_type) return arg;
        internal::value<Context> &val = arg.value_;
        val                           = values_[index];
        return arg;
    }

public:
    basic_format_args() : types_(0) {}

    /**
     \rst
     Constructs a `basic_format_args` object from `~fmt::format_arg_store`.
     \endrst
     */
    template <typename... Args>
    basic_format_args(const format_arg_store<Context, Args...> &store) : types_(static_cast<unsigned long long>(store.TYPES)) {
        set_data(store.data_);
    }

    /**
     \rst
     Constructs a `basic_format_args` object from a dynamic set of arguments.
     \endrst
     */
    basic_format_args(const format_arg *args, size_type count) : types_(internal::is_unpacked_bit | count) { set_data(args); }

    /** Returns the argument at specified index. */
    format_arg get(size_type index) const {
        format_arg arg = do_get(index);
        if (arg.type_ == internal::named_arg_type) arg = arg.value_.as_named_arg().template deserialize<Context>();
        return arg;
    }

    size_type max_size() const {
        unsigned long long max_packed = internal::max_packed_args;
        return static_cast<size_type>(is_packed() ? max_packed : types_ & ~internal::is_unpacked_bit);
    }
};

/** An alias to ``basic_format_args<context>``. */
// It is a separate type rather than a typedef to make symbols readable.
struct format_args : basic_format_args<format_context> {
    template <typename... Args>
    format_args(Args &&... arg) : basic_format_args<format_context>(std::forward<Args>(arg)...) {}
};
struct wformat_args : basic_format_args<wformat_context> {
    template <typename... Args>
    wformat_args(Args &&... arg) : basic_format_args<wformat_context>(std::forward<Args>(arg)...) {}
};

#define FMT_ENABLE_IF_T(B, T) typename std::enable_if<B, T>::type

#ifndef FMT_USE_ALIAS_TEMPLATES
#define FMT_USE_ALIAS_TEMPLATES FMT_HAS_FEATURE(cxx_alias_templates)
#endif
#if FMT_USE_ALIAS_TEMPLATES
/** String's character type. */
template <typename S>
using char_t = FMT_ENABLE_IF_T(internal::is_string<S>::value, typename internal::char_t<S>::type);
#define FMT_CHAR(S) fmt::char_t<S>
#else
template <typename S>
struct char_t : std::enable_if<internal::is_string<S>::value, typename internal::char_t<S>::type> {};
#define FMT_CHAR(S) typename char_t<S>::type
#endif

namespace internal {
template <typename Char>
struct named_arg_base {
    basic_string_view<Char> name;

    // Serialized value<context>.
    mutable char data[sizeof(basic_format_arg<typename buffer_context<Char>::type>)];

    named_arg_base(basic_string_view<Char> nm) : name(nm) {}

    template <typename Context>
    basic_format_arg<Context> deserialize() const {
        basic_format_arg<Context> arg;
        std::memcpy(&arg, data, sizeof(basic_format_arg<Context>));
        return arg;
    }
};

template <typename T, typename Char>
struct named_arg : named_arg_base<Char> {
    const T &value;

    named_arg(basic_string_view<Char> name, const T &val) : named_arg_base<Char>(name), value(val) {}
};

template <typename... Args, typename S>
inline typename std::enable_if<!is_compile_string<S>::value>::type check_format_string(const S &) {}
template <typename... Args, typename S>
typename std::enable_if<is_compile_string<S>::value>::type check_format_string(S);

template <typename S, typename... Args>
struct checked_args : format_arg_store<typename buffer_context<FMT_CHAR(S)>::type, Args...> {
    typedef typename buffer_context<FMT_CHAR(S)>::type context;

    checked_args(const S &format_str, const Args &... args) : format_arg_store<context, Args...>(args...) {
        internal::check_format_string<Args...>(format_str);
    }

    basic_format_args<context> operator*() const { return *this; }
};

template <typename Char>
std::basic_string<Char> vformat(basic_string_view<Char> format_str, basic_format_args<typename buffer_context<Char>::type> args);

template <typename Char>
typename buffer_context<Char>::type::iterator vformat_to(internal::basic_buffer<Char> &buf, basic_string_view<Char> format_str,
                                                         basic_format_args<typename buffer_context<Char>::type> args);
}  // namespace internal

/**
  \rst
  Returns a named argument to be used in a formatting function.

  **Example**::

    fmt::print("Elapsed time: {s:.2f} seconds", fmt::arg("s", 1.23));
  \endrst
 */
template <typename T>
inline internal::named_arg<T, char> arg(string_view name, const T &arg) {
    return {name, arg};
}

template <typename T>
inline internal::named_arg<T, wchar_t> arg(wstring_view name, const T &arg) {
    return {name, arg};
}

// Disable nested named arguments, e.g. ``arg("a", arg("b", 42))``.
template <typename S, typename T, typename Char>
void arg(S, internal::named_arg<T, Char>) = delete;

template <typename Container>
struct is_contiguous : std::false_type {};

template <typename Char>
struct is_contiguous<std::basic_string<Char>> : std::true_type {};

template <typename Char>
struct is_contiguous<internal::basic_buffer<Char>> : std::true_type {};

/** Formats a string and writes the output to ``out``. */
template <typename Container, typename S>
typename std::enable_if<is_contiguous<Container>::value, std::back_insert_iterator<Container>>::type vformat_to(
    std::back_insert_iterator<Container> out, const S &format_str, basic_format_args<typename buffer_context<FMT_CHAR(S)>::type> args) {
    internal::container_buffer<Container> buf(internal::get_container(out));
    internal::vformat_to(buf, to_string_view(format_str), args);
    return out;
}

template <typename Container, typename S, typename... Args>
inline typename std::enable_if<is_contiguous<Container>::value && internal::is_string<S>::value, std::back_insert_iterator<Container>>::type format_to(
    std::back_insert_iterator<Container> out, const S &format_str, const Args &... args) {
    internal::checked_args<S, Args...> ca(format_str, args...);
    return vformat_to(out, to_string_view(format_str), *ca);
}

template <typename S, typename Char = FMT_CHAR(S)>
inline std::basic_string<Char> vformat(const S &format_str, basic_format_args<typename buffer_context<Char>::type> args) {
    return internal::vformat(to_string_view(format_str), args);
}

/**
  \rst
  Formats arguments and returns the result as a string.

  **Example**::

    #include <fmt/core.h>
    std::string message = fmt::format("The answer is {}", 42);
  \endrst
*/
template <typename S, typename... Args>
inline std::basic_string<FMT_CHAR(S)> format(const S &format_str, const Args &... args) {
    return internal::vformat(to_string_view(format_str), *internal::checked_args<S, Args...>(format_str, args...));
}

FMT_API void vprint(std::FILE *f, string_view format_str, format_args args);
FMT_API void vprint(std::FILE *f, wstring_view format_str, wformat_args args);

/**
  \rst
  Prints formatted data to the file *f*. For wide format strings,
  *f* should be in wide-oriented mode set via ``fwide(f, 1)`` or
  ``_setmode(_fileno(f), _O_U8TEXT)`` on Windows.

  **Example**::

    fmt::print(stderr, "Don't {}!", "panic");
  \endrst
 */
template <typename S, typename... Args>
inline FMT_ENABLE_IF_T(internal::is_string<S>::value, void) print(std::FILE *f, const S &format_str, const Args &... args) {
    vprint(f, to_string_view(format_str), internal::checked_args<S, Args...>(format_str, args...));
}

FMT_API void vprint(string_view format_str, format_args args);
FMT_API void vprint(wstring_view format_str, wformat_args args);

/**
  \rst
  Prints formatted data to ``stdout``.

  **Example**::

    fmt::print("Elapsed time: {0:.2f} seconds", 1.23);
  \endrst
 */
template <typename S, typename... Args>
inline FMT_ENABLE_IF_T(internal::is_string<S>::value, void) print(const S &format_str, const Args &... args) {
    vprint(to_string_view(format_str), internal::checked_args<S, Args...>(format_str, args...));
}
FMT_END_NAMESPACE

#endif  // FMT_CORE_H_

#if FMT_GCC_VERSION >= 406 || FMT_CLANG_VERSION
#pragma GCC diagnostic push

// Disable the warning about declaration shadowing because it affects too
// many valid cases.
#pragma GCC diagnostic ignored "-Wshadow"

// Disable the warning about nonliteral format strings because we construct
// them dynamically when falling back to snprintf for FP formatting.
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

#if FMT_CLANG_VERSION
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

#ifdef _SECURE_SCL
#define FMT_SECURE_SCL _SECURE_SCL
#else
#define FMT_SECURE_SCL 0
#endif

#if FMT_SECURE_SCL
#include <iterator>
#endif

#ifdef __has_builtin
#define FMT_HAS_BUILTIN(x) __has_builtin(x)
#else
#define FMT_HAS_BUILTIN(x) 0
#endif

#ifdef __GNUC_LIBSTD__
#define FMT_GNUC_LIBSTD_VERSION (__GNUC_LIBSTD__ * 100 + __GNUC_LIBSTD_MINOR__)
#endif

#define FMT_THROW(x)                  \
    do {                              \
        static_cast<void>(sizeof(x)); \
        assert(false);                \
    } while (false);

#ifndef FMT_USE_USER_DEFINED_LITERALS
// For Intel's compiler and NVIDIA's compiler both it and the system gcc/msc
// must support UDLs.
#if (FMT_HAS_FEATURE(cxx_user_literals) || FMT_GCC_VERSION >= 407 || FMT_MSC_VER >= 1900) \
    && (!(FMT_ICC_VERSION || FMT_CUDA_VERSION) || FMT_ICC_VERSION >= 1500 || FMT_CUDA_VERSION >= 700)
#define FMT_USE_USER_DEFINED_LITERALS 1
#else
#define FMT_USE_USER_DEFINED_LITERALS 0
#endif
#endif

// EDG C++ Front End based compilers (icc, nvcc) do not currently support UDL
// templates.
#if FMT_USE_USER_DEFINED_LITERALS && FMT_ICC_VERSION == 0 && FMT_CUDA_VERSION == 0 \
    && ((FMT_GCC_VERSION >= 600 && __cplusplus >= 201402L) || (defined(FMT_CLANG_VERSION) && FMT_CLANG_VERSION >= 304))
#define FMT_UDL_TEMPLATE 1
#else
#define FMT_UDL_TEMPLATE 0
#endif

#if FMT_HAS_GXX_CXX11 || FMT_HAS_FEATURE(cxx_trailing_return) || FMT_MSC_VER >= 1600
#define FMT_USE_TRAILING_RETURN 1
#else
#define FMT_USE_TRAILING_RETURN 0
#endif

#ifndef FMT_USE_GRISU
#define FMT_USE_GRISU 0
//# define FMT_USE_GRISU std::numeric_limits<double>::is_iec559
#endif

// __builtin_clz is broken in clang with Microsoft CodeGen:
// https://github.com/fmtlib/fmt/issues/519
#ifndef _MSC_VER
#if FMT_GCC_VERSION >= 400 || FMT_HAS_BUILTIN(__builtin_clz)
#define FMT_BUILTIN_CLZ(n) __builtin_clz(n)
#endif

#if FMT_GCC_VERSION >= 400 || FMT_HAS_BUILTIN(__builtin_clzll)
#define FMT_BUILTIN_CLZLL(n) __builtin_clzll(n)
#endif
#endif

// Some compilers masquerade as both MSVC and GCC-likes or otherwise support
// __builtin_clz and __builtin_clzll, so only define FMT_BUILTIN_CLZ using the
// MSVC intrinsics if the clz and clzll builtins are not available.
#if FMT_MSC_VER && !defined(FMT_BUILTIN_CLZLL) && !defined(_MANAGED)
#include <intrin.h>  // _BitScanReverse, _BitScanReverse64

FMT_BEGIN_NAMESPACE
namespace internal {
// Avoid Clang with Microsoft CodeGen's -Wunknown-pragmas warning.
#ifndef __clang__
#pragma intrinsic(_BitScanReverse)
#endif
inline uint32_t clz(uint32_t x) {
    unsigned long r = 0;
    _BitScanReverse(&r, x);

    assert(x != 0);
    // Static analysis complains about using uninitialized data
    // "r", but the only way that can happen is if "x" is 0,
    // which the callers guarantee to not happen.
#pragma warning(suppress : 6102)
    return 31 - r;
}
#define FMT_BUILTIN_CLZ(n) fmt::internal::clz(n)

#if defined(_WIN64) && !defined(__clang__)
#pragma intrinsic(_BitScanReverse64)
#endif

inline uint32_t clzll(uint64_t x) {
    unsigned long r = 0;
#ifdef _WIN64
    _BitScanReverse64(&r, x);
#else
    // Scan the high 32 bits.
    if (_BitScanReverse(&r, static_cast<uint32_t>(x >> 32))) return 63 - (r + 32);

    // Scan the low 32 bits.
    _BitScanReverse(&r, static_cast<uint32_t>(x));
#endif

    assert(x != 0);
    // Static analysis complains about using uninitialized data
    // "r", but the only way that can happen is if "x" is 0,
    // which the callers guarantee to not happen.
#pragma warning(suppress : 6102)
    return 63 - r;
}
#define FMT_BUILTIN_CLZLL(n) fmt::internal::clzll(n)
}  // namespace internal
FMT_END_NAMESPACE
#endif

FMT_BEGIN_NAMESPACE
namespace internal {

// An equivalent of `*reinterpret_cast<Dest*>(&source)` that doesn't produce
// undefined behavior (e.g. due to type aliasing).
// Example: uint64_t d = bit_cast<uint64_t>(2.718);
template <typename Dest, typename Source>
inline Dest bit_cast(const Source &source) {
    static_assert(sizeof(Dest) == sizeof(Source), "size mismatch");
    Dest dest;
    std::memcpy(&dest, &source, sizeof(dest));
    return dest;
}

// An implementation of begin and end for pre-C++11 compilers such as gcc 4.
template <typename C>
constexpr auto begin(const C &c) -> decltype(c.begin()) {
    return c.begin();
}
template <typename T, std::size_t N>
constexpr T *begin(T (&array)[N]) noexcept {
    return array;
}
template <typename C>
constexpr auto end(const C &c) -> decltype(c.end()) {
    return c.end();
}
template <typename T, std::size_t N>
constexpr T *end(T (&array)[N]) noexcept {
    return array + N;
}

// For std::result_of in gcc 4.4.
template <typename Result>
struct function {
    template <typename T>
    struct result {
        typedef Result type;
    };
};

struct dummy_int {
    int data[2];
        operator int() const { return 0; }
};
typedef std::numeric_limits<internal::dummy_int> fputil;

// Dummy implementations of system functions called if the latter are not
// available.
inline dummy_int isinf(...) { return dummy_int(); }
inline dummy_int _finite(...) { return dummy_int(); }
inline dummy_int isnan(...) { return dummy_int(); }
inline dummy_int _isnan(...) { return dummy_int(); }

template <typename Allocator>
typename Allocator::value_type *allocate(Allocator &alloc, std::size_t n) {
#if __cplusplus >= 201103L || FMT_MSC_VER >= 1700
    return std::allocator_traits<Allocator>::allocate(alloc, n);
#else
    return alloc.allocate(n);
#endif
}

// A helper function to suppress bogus "conditional expression is constant"
// warnings.
template <typename T>
inline T const_check(T value) {
    return value;
}
}  // namespace internal
FMT_END_NAMESPACE

namespace std {
// Standard permits specialization of std::numeric_limits. This specialization
// is used to resolve ambiguity between isinf and std::isinf in glibc:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=48891
// and the same for isnan.
template <>
class numeric_limits<fmt::internal::dummy_int> : public std::numeric_limits<int> {
public:
    // Portable version of isinf.
    template <typename T>
    static bool isinfinity(T x) {
        using namespace fmt::internal;
        // The resolution "priority" is:
        // isinf macro > std::isinf > ::isinf > fmt::internal::isinf
        if (const_check(sizeof(isinf(x)) != sizeof(fmt::internal::dummy_int))) return isinf(x) != 0;
        return !_finite(static_cast<double>(x));
    }

    // Portable version of isnan.
    template <typename T>
    static bool isnotanumber(T x) {
        using namespace fmt::internal;
        if (const_check(sizeof(isnan(x)) != sizeof(fmt::internal::dummy_int))) return isnan(x) != 0;
        return _isnan(static_cast<double>(x)) != 0;
    }
};
}  // namespace std

FMT_BEGIN_NAMESPACE
template <typename Range>
class basic_writer;

template <typename OutputIt, typename T = typename OutputIt::value_type>
class output_range {
private:
    OutputIt it_;

    // Unused yet.
    typedef void sentinel;
    sentinel     end() const;

public:
    typedef OutputIt iterator;
    typedef T        value_type;

    explicit output_range(OutputIt it) : it_(it) {}
    OutputIt begin() const { return it_; }
};

// A range where begin() returns back_insert_iterator.
template <typename Container>
class back_insert_range : public output_range<std::back_insert_iterator<Container>> {
    typedef output_range<std::back_insert_iterator<Container>> base;

public:
    typedef typename Container::value_type value_type;

    back_insert_range(Container &c) : base(std::back_inserter(c)) {}
    back_insert_range(typename base::iterator it) : base(it) {}
};

typedef basic_writer<back_insert_range<internal::buffer>>  writer;
typedef basic_writer<back_insert_range<internal::wbuffer>> wwriter;

namespace internal {

#if FMT_SECURE_SCL
template <typename T>
struct checked {
    typedef stdext::checked_array_iterator<T *> type;
};

// Make a checked iterator to avoid warnings on MSVC.
template <typename T>
inline stdext::checked_array_iterator<T *> make_checked(T *p, std::size_t size) {
    return {p, size};
}
#else
template <typename T>
struct checked {
    typedef T *type;
};
template <typename T>
inline T *make_checked(T *p, std::size_t) {
    return p;
}
#endif

template <typename T>
template <typename U>
void basic_buffer<T>::append(const U *begin, const U *end) {
    std::size_t new_size = size_ + internal::to_unsigned(end - begin);
    reserve(new_size);
    std::uninitialized_copy(begin, end, internal::make_checked(ptr_, capacity_) + size_);
    size_ = new_size;
}
}  // namespace internal

// C++20 feature test, since r346892 Clang considers char8_t a fundamental
// type in this mode. If this is the case __cpp_char8_t will be defined.
#if !defined(__cpp_char8_t)
// A UTF-8 code unit type.
enum char8_t : unsigned char {};
#endif

// A UTF-8 string view.
class u8string_view : public basic_string_view<char8_t> {
public:
    typedef char8_t char_type;

    u8string_view(const char *s) : basic_string_view<char8_t>(reinterpret_cast<const char8_t *>(s)) {}
    u8string_view(const char *s, size_t count) noexcept : basic_string_view<char8_t>(reinterpret_cast<const char8_t *>(s), count) {}
};

// The number of characters to store in the basic_memory_buffer object itself
// to avoid dynamic memory allocation.
enum { inline_buffer_size = 500 };

/**
  \rst
  A dynamically growing memory buffer for trivially copyable/constructible types
  with the first ``SIZE`` elements stored in the object itself.

  You can use one of the following typedefs for common character types:

  +----------------+------------------------------+
  | Type           | Definition                   |
  +================+==============================+
  | memory_buffer  | basic_memory_buffer<char>    |
  +----------------+------------------------------+
  | wmemory_buffer | basic_memory_buffer<wchar_t> |
  +----------------+------------------------------+

  **Example**::

     fmt::memory_buffer out;
     format_to(out, "The answer is {}.", 42);

  This will append the following output to the ``out`` object:

  .. code-block:: none

     The answer is 42.

  The output can be converted to an ``std::string`` with ``to_string(out)``.
  \endrst
 */
template <typename T, std::size_t SIZE = inline_buffer_size, typename Allocator = std::allocator<T>>
class basic_memory_buffer : private Allocator, public internal::basic_buffer<T> {
private:
    T store_[SIZE];

    // Deallocate memory allocated by the buffer.
    void deallocate() {
        T *data = this->data();
        if (data != store_) Allocator::deallocate(data, this->capacity());
    }

protected:
    void grow(std::size_t size) override;

public:
    typedef T        value_type;
    typedef const T &const_reference;

    explicit basic_memory_buffer(const Allocator &alloc = Allocator()) : Allocator(alloc) { this->set(store_, SIZE); }
    ~basic_memory_buffer() override { deallocate(); }

private:
    // Move data from other to this buffer.
    void move(basic_memory_buffer &other) {
        Allocator &this_alloc = *this, &other_alloc = other;
        this_alloc       = std::move(other_alloc);
        T *         data = other.data();
        std::size_t size = other.size(), capacity = other.capacity();
        if (data == other.store_) {
            this->set(store_, capacity);
            std::uninitialized_copy(other.store_, other.store_ + size, internal::make_checked(store_, capacity));
        } else {
            this->set(data, capacity);
            // Set pointer to the inline array so that delete is not called
            // when deallocating.
            other.set(other.store_, 0);
        }
        this->resize(size);
    }

public:
    /**
      \rst
      Constructs a :class:`fmt::basic_memory_buffer` object moving the content
      of the other object to it.
      \endrst
     */
    basic_memory_buffer(basic_memory_buffer &&other) { move(other); }

    /**
      \rst
      Moves the content of the other ``basic_memory_buffer`` object to this one.
      \endrst
     */
    basic_memory_buffer &operator=(basic_memory_buffer &&other) {
        assert(this != &other);
        deallocate();
        move(other);
        return *this;
    }

    // Returns a copy of the allocator associated with this buffer.
    Allocator get_allocator() const { return *this; }
};

template <typename T, std::size_t SIZE, typename Allocator>
void basic_memory_buffer<T, SIZE, Allocator>::grow(std::size_t size) {
    std::size_t old_capacity = this->capacity();
    std::size_t new_capacity = old_capacity + old_capacity / 2;
    if (size > new_capacity) new_capacity = size;
    T *old_data = this->data();
    T *new_data = internal::allocate<Allocator>(*this, new_capacity);
    // The following code doesn't throw, so the raw pointer above doesn't leak.
    std::uninitialized_copy(old_data, old_data + this->size(), internal::make_checked(new_data, new_capacity));
    this->set(new_data, new_capacity);
    // deallocate must not throw according to the standard, but even if it does,
    // the buffer already uses the new storage and will deallocate it in
    // destructor.
    if (old_data != store_) Allocator::deallocate(old_data, old_capacity);
}

typedef basic_memory_buffer<char>    memory_buffer;
typedef basic_memory_buffer<wchar_t> wmemory_buffer;

namespace internal {

template <typename Char>
struct char_traits;

template <>
struct char_traits<char> {
    // Formats a floating-point number.
    template <typename T>
    FMT_API static int format_float(char *buffer, std::size_t size, const char *format, int precision, T value);
};

template <>
struct char_traits<wchar_t> {
    template <typename T>
    FMT_API static int format_float(wchar_t *buffer, std::size_t size, const wchar_t *format, int precision, T value);
};

template <typename Container>
inline typename std::enable_if<is_contiguous<Container>::value, typename checked<typename Container::value_type>::type>::type reserve(
    std::back_insert_iterator<Container> &it, std::size_t n) {
    Container & c    = internal::get_container(it);
    std::size_t size = c.size();
    c.resize(size + n);
    return make_checked(&c[size], n);
}

template <typename Iterator>
inline Iterator &reserve(Iterator &it, std::size_t) {
    return it;
}

template <typename Char>
class null_terminating_iterator;

template <typename Char>
constexpr const Char *pointer_from(null_terminating_iterator<Char> it);

// An output iterator that counts the number of objects written to it and
// discards them.
template <typename T>
class counting_iterator {
private:
    std::size_t count_;
    mutable T   blackhole_;

public:
    typedef std::output_iterator_tag iterator_category;
    typedef T                        value_type;
    typedef std::ptrdiff_t           difference_type;
    typedef T *                      pointer;
    typedef T &                      reference;
    typedef counting_iterator        _Unchecked_type;  // Mark iterator as checked.

    counting_iterator() : count_(0) {}

    std::size_t count() const { return count_; }

    counting_iterator &operator++() {
        ++count_;
        return *this;
    }

    counting_iterator operator++(int) {
        auto it = *this;
        ++*this;
        return it;
    }

    T &operator*() const { return blackhole_; }
};

template <typename OutputIt>
class truncating_iterator_base {
protected:
    OutputIt    out_;
    std::size_t limit_;
    std::size_t count_;

    truncating_iterator_base(OutputIt out, std::size_t limit) : out_(out), limit_(limit), count_(0) {}

public:
    typedef std::output_iterator_tag iterator_category;
    typedef void                     difference_type;
    typedef void                     pointer;
    typedef void                     reference;
    typedef truncating_iterator_base _Unchecked_type;  // Mark iterator as checked.

    OutputIt    base() const { return out_; }
    std::size_t count() const { return count_; }
};

// An output iterator that truncates the output and counts the number of objects
// written to it.
template <typename OutputIt, typename Enable = typename std::is_void<typename std::iterator_traits<OutputIt>::value_type>::type>
class truncating_iterator;

template <typename OutputIt>
class truncating_iterator<OutputIt, std::false_type> : public truncating_iterator_base<OutputIt> {
    typedef std::iterator_traits<OutputIt> traits;

    mutable typename traits::value_type blackhole_;

public:
    typedef typename traits::value_type value_type;

    truncating_iterator(OutputIt out, std::size_t limit) : truncating_iterator_base<OutputIt>(out, limit) {}

    truncating_iterator &operator++() {
        if (this->count_++ < this->limit_) ++this->out_;
        return *this;
    }

    truncating_iterator operator++(int) {
        auto it = *this;
        ++*this;
        return it;
    }

    value_type &operator*() const { return this->count_ < this->limit_ ? *this->out_ : blackhole_; }
};

template <typename OutputIt>
class truncating_iterator<OutputIt, std::true_type> : public truncating_iterator_base<OutputIt> {
public:
    typedef typename OutputIt::container_type::value_type value_type;

    truncating_iterator(OutputIt out, std::size_t limit) : truncating_iterator_base<OutputIt>(out, limit) {}

    truncating_iterator &operator=(value_type val) {
        if (this->count_++ < this->limit_) this->out_ = val;
        return *this;
    }

    truncating_iterator &operator++() { return *this; }
    truncating_iterator &operator++(int) { return *this; }
    truncating_iterator &operator*() { return *this; }
};

// Returns true if value is negative, false otherwise.
// Same as (value < 0) but doesn't produce warnings if T is an unsigned type.
template <typename T>
constexpr typename std::enable_if<std::numeric_limits<T>::is_signed, bool>::type is_negative(T value) {
    return value < 0;
}
template <typename T>
constexpr typename std::enable_if<!std::numeric_limits<T>::is_signed, bool>::type is_negative(T) {
    return false;
}

template <typename T>
struct int_traits {
    // Smallest of uint32_t and uint64_t that is large enough to represent
    // all values of T.
    typedef typename std::conditional<std::numeric_limits<T>::digits <= 32, uint32_t, uint64_t>::type main_type;
};

// Static data is placed in this class template to allow header-only
// configuration.
template <typename T = void>
struct FMT_API basic_data {
    static const uint32_t POWERS_OF_10_32[];
    static const uint32_t ZERO_OR_POWERS_OF_10_32[];
    static const uint64_t ZERO_OR_POWERS_OF_10_64[];
    static const uint64_t POW10_SIGNIFICANDS[];
    static const int16_t  POW10_EXPONENTS[];
    static const char     DIGITS[];
    static const char     FOREGROUND_COLOR[];
    static const char     BACKGROUND_COLOR[];
    static const char     RESET_COLOR[];
    static const wchar_t  WRESET_COLOR[];
};

typedef basic_data<> data;

#ifdef FMT_BUILTIN_CLZLL
// Returns the number of decimal digits in n. Leading zeros are not counted
// except for n == 0 in which case count_digits returns 1.
inline int count_digits(uint64_t n) {
    // Based on http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
    // and the benchmark https://github.com/localvoid/cxx-benchmark-count-digits.
    int t = (64 - FMT_BUILTIN_CLZLL(n | 1)) * 1233 >> 12;
    return t - (n < data::ZERO_OR_POWERS_OF_10_64[t]) + 1;
}
#else
// Fallback version of count_digits used when __builtin_clz is not available.
inline int count_digits(uint64_t n) {
    int count = 1;
    for (;;) {
        // Integer division is slow so do it for a group of four digits instead
        // of for every digit. The idea comes from the talk by Alexandrescu
        // "Three Optimization Tips for C++". See speed-test for a comparison.
        if (n < 10) return count;
        if (n < 100) return count + 1;
        if (n < 1000) return count + 2;
        if (n < 10000) return count + 3;
        n /= 10000u;
        count += 4;
    }
}
#endif

template <typename Char>
inline size_t count_code_points(basic_string_view<Char> s) {
    return s.size();
}

// Counts the number of code points in a UTF-8 string.
FMT_API size_t count_code_points(basic_string_view<char8_t> s);

inline char8_t to_char8_t(char c) { return static_cast<char8_t>(c); }

template <typename InputIt, typename OutChar>
struct needs_conversion
    : std::integral_constant<bool, std::is_same<typename std::iterator_traits<InputIt>::value_type, char>::value && std::is_same<OutChar, char8_t>::value> {};

template <typename OutChar, typename InputIt, typename OutputIt>
typename std::enable_if<!needs_conversion<InputIt, OutChar>::value, OutputIt>::type copy_str(InputIt begin, InputIt end, OutputIt it) {
    return std::copy(begin, end, it);
}

template <typename OutChar, typename InputIt, typename OutputIt>
typename std::enable_if<needs_conversion<InputIt, OutChar>::value, OutputIt>::type copy_str(InputIt begin, InputIt end, OutputIt it) {
    return std::transform(begin, end, it, to_char8_t);
}

#if FMT_HAS_CPP_ATTRIBUTE(always_inline)
#define FMT_ALWAYS_INLINE __attribute__((always_inline))
#else
#define FMT_ALWAYS_INLINE
#endif

template <typename Handler>
inline char *lg(uint32_t n, Handler h) FMT_ALWAYS_INLINE;

// Computes g = floor(log10(n)) and calls h.on<g>(n);
template <typename Handler>
inline char *lg(uint32_t n, Handler h) {
    return n < 100 ? n < 10 ? h.template on<0>(n) : h.template on<1>(n)
                   : n < 1000000 ? n < 10000 ? n < 1000 ? h.template on<2>(n) : h.template on<3>(n) : n < 100000 ? h.template on<4>(n) : h.template on<5>(n)
                                 : n < 100000000 ? n < 10000000 ? h.template on<6>(n) : h.template on<7>(n)
                                                 : n < 1000000000 ? h.template on<8>(n) : h.template on<9>(n);
}

// An lg handler that formats a decimal number.
// Usage: lg(n, decimal_formatter(buffer));
class decimal_formatter {
private:
    char *buffer_;

    void write_pair(unsigned N, uint32_t index) { std::memcpy(buffer_ + N, data::DIGITS + index * 2, 2); }

public:
    explicit decimal_formatter(char *buf) : buffer_(buf) {}

    template <unsigned N>
    char *on(uint32_t u) {
        if (N == 0) {
            *buffer_ = static_cast<char>(u) + '0';
        } else if (N == 1) {
            write_pair(0, u);
        } else {
            // The idea of using 4.32 fixed-point numbers is based on
            // https://github.com/jeaiii/itoa
            unsigned n = N - 1;
            unsigned a = n / 5 * n * 53 / 16;
            uint64_t t = ((1ULL << (32 + a)) / data::ZERO_OR_POWERS_OF_10_32[n] + 1 - n / 9);
            t          = ((t * u) >> a) + n / 5 * 4;
            write_pair(0, t >> 32);
            for (unsigned i = 2; i < N; i += 2) {
                t = 100ULL * static_cast<uint32_t>(t);
                write_pair(i, t >> 32);
            }
            if (N % 2 == 0) { buffer_[N] = static_cast<char>((10ULL * static_cast<uint32_t>(t)) >> 32) + '0'; }
        }
        return buffer_ += N + 1;
    }
};

// An lg handler that formats a decimal number with a terminating null.
class decimal_formatter_null : public decimal_formatter {
public:
    explicit decimal_formatter_null(char *buf) : decimal_formatter(buf) {}

    template <unsigned N>
    char *on(uint32_t u) {
        char *buf = decimal_formatter::on<N>(u);
        *buf      = '\0';
        return buf;
    }
};

#ifdef FMT_BUILTIN_CLZ
// Optional version of count_digits for better performance on 32-bit platforms.
inline int count_digits(uint32_t n) {
    int t = (32 - FMT_BUILTIN_CLZ(n | 1)) * 1233 >> 12;
    return t - (n < data::ZERO_OR_POWERS_OF_10_32[t]) + 1;
}
#endif

// A functor that doesn't add a thousands separator.
struct no_thousands_sep {
    typedef char char_type;

    template <typename Char>
    void operator()(Char *) {}

    enum { size = 0 };
};

// A functor that adds a thousands separator.
template <typename Char>
class add_thousands_sep {
private:
    basic_string_view<Char> sep_;

    // Index of a decimal digit with the least significant digit having index 0.
    unsigned digit_index_;

public:
    typedef Char char_type;

    explicit add_thousands_sep(basic_string_view<Char> sep) : sep_(sep), digit_index_(0) {}

    void operator()(Char *&buffer) {
        if (++digit_index_ % 3 != 0) return;
        buffer -= sep_.size();
        std::uninitialized_copy(sep_.data(), sep_.data() + sep_.size(), internal::make_checked(buffer, sep_.size()));
    }

    enum { size = 1 };
};

template <typename Char>
FMT_API Char thousands_sep_impl(locale_ref loc);

template <typename Char>
inline Char thousands_sep(locale_ref loc) {
    return Char(thousands_sep_impl<char>(loc));
}

template <>
inline wchar_t thousands_sep(locale_ref loc) {
    return thousands_sep_impl<wchar_t>(loc);
}

// Formats a decimal unsigned integer value writing into buffer.
// thousands_sep is a functor that is called after writing each char to
// add a thousands separator if necessary.
template <typename UInt, typename Char, typename ThousandsSep>
inline Char *format_decimal(Char *buffer, UInt value, int num_digits, ThousandsSep thousands_sep) {
    FMT_ASSERT(num_digits >= 0, "invalid digit count");
    buffer += num_digits;
    Char *end = buffer;
    while (value >= 100) {
        // Integer division is slow so do it for a group of two digits instead
        // of for every digit. The idea comes from the talk by Alexandrescu
        // "Three Optimization Tips for C++". See speed-test for a comparison.
        unsigned index = static_cast<unsigned>((value % 100) * 2);
        value /= 100;
        *--buffer = static_cast<Char>(data::DIGITS[index + 1]);
        thousands_sep(buffer);
        *--buffer = static_cast<Char>(data::DIGITS[index]);
        thousands_sep(buffer);
    }
    if (value < 10) {
        *--buffer = static_cast<Char>('0' + value);
        return end;
    }
    unsigned index = static_cast<unsigned>(value * 2);
    *--buffer      = static_cast<Char>(data::DIGITS[index + 1]);
    thousands_sep(buffer);
    *--buffer = static_cast<Char>(data::DIGITS[index]);
    return end;
}

template <typename OutChar, typename UInt, typename Iterator, typename ThousandsSep>
inline Iterator format_decimal(Iterator out, UInt value, int num_digits, ThousandsSep sep) {
    FMT_ASSERT(num_digits >= 0, "invalid digit count");
    typedef typename ThousandsSep::char_type char_type;
    // Buffer should be large enough to hold all digits (<= digits10 + 1).
    enum { max_size = std::numeric_limits<UInt>::digits10 + 1 };
    FMT_ASSERT(ThousandsSep::size <= 1, "invalid separator");
    char_type buffer[max_size + max_size / 3];
    auto      end = format_decimal(buffer, value, num_digits, sep);
    return internal::copy_str<OutChar>(buffer, end, out);
}

template <typename OutChar, typename It, typename UInt>
inline It format_decimal(It out, UInt value, int num_digits) {
    return format_decimal<OutChar>(out, value, num_digits, no_thousands_sep());
}

template <unsigned BASE_BITS, typename Char, typename UInt>
inline Char *format_uint(Char *buffer, UInt value, int num_digits, bool upper = false) {
    buffer += num_digits;
    Char *end = buffer;
    do {
        const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        unsigned    digit  = (value & ((1 << BASE_BITS) - 1));
        *--buffer          = static_cast<Char>(BASE_BITS < 4 ? static_cast<char>('0' + digit) : digits[digit]);
    } while ((value >>= BASE_BITS) != 0);
    return end;
}

template <unsigned BASE_BITS, typename Char, typename It, typename UInt>
inline It format_uint(It out, UInt value, int num_digits, bool upper = false) {
    // Buffer should be large enough to hold all digits (digits / BASE_BITS + 1)
    // and null.
    char buffer[std::numeric_limits<UInt>::digits / BASE_BITS + 2];
    format_uint<BASE_BITS>(buffer, value, num_digits, upper);
    return internal::copy_str<Char>(buffer, buffer + num_digits, out);
}

#ifndef _WIN32
#define FMT_USE_WINDOWS_H 0
#elif !defined(FMT_USE_WINDOWS_H)
#define FMT_USE_WINDOWS_H 1
#endif

// Define FMT_USE_WINDOWS_H to 0 to disable use of windows.h.
// All the functionality that relies on it will be disabled too.
#if FMT_USE_WINDOWS_H
// A converter from UTF-8 to UTF-16.
// It is only provided for Windows since other systems support UTF-8 natively.
class utf8_to_utf16 {
private:
    wmemory_buffer buffer_;

public:
    FMT_API explicit utf8_to_utf16(string_view s);
                   operator wstring_view() const { return wstring_view(&buffer_[0], size()); }
    size_t         size() const { return buffer_.size() - 1; }
    const wchar_t *c_str() const { return &buffer_[0]; }
    std::wstring   str() const { return std::wstring(&buffer_[0], size()); }
};

// A converter from UTF-16 to UTF-8.
// It is only provided for Windows since other systems support UTF-8 natively.
class utf16_to_utf8 {
private:
    memory_buffer buffer_;

public:
    utf16_to_utf8() {}
    FMT_API explicit utf16_to_utf8(wstring_view s);
                operator string_view() const { return string_view(&buffer_[0], size()); }
    size_t      size() const { return buffer_.size() - 1; }
    const char *c_str() const { return &buffer_[0]; }
    std::string str() const { return std::string(&buffer_[0], size()); }

    // Performs conversion returning a system error code instead of
    // throwing exception on conversion error. This method may still throw
    // in case of memory allocation error.
    FMT_API int convert(wstring_view s);
};

FMT_API void format_windows_error(fmt::internal::buffer &out, int error_code, fmt::string_view message) noexcept;
#endif

template <typename T = void>
struct null {};
}  // namespace internal

enum alignment { ALIGN_DEFAULT, ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER, ALIGN_NUMERIC };

// Flags.
enum { SIGN_FLAG = 1, PLUS_FLAG = 2, MINUS_FLAG = 4, HASH_FLAG = 8 };

// An alignment specifier.
struct align_spec {
    unsigned width_;
    // Fill is always wchar_t and cast to char if necessary to avoid having
    // two specialization of AlignSpec and its subclasses.
    wchar_t   fill_;
    alignment align_;

    constexpr align_spec() : width_(0), fill_(' '), align_(ALIGN_DEFAULT) {}
    constexpr unsigned  width() const { return width_; }
    constexpr wchar_t   fill() const { return fill_; }
    constexpr alignment align() const { return align_; }
};

struct core_format_specs {
    int           precision;
    uint_least8_t flags;
    char          type;

    constexpr core_format_specs() : precision(-1), flags(0), type(0) {}
    constexpr bool has(unsigned f) const { return (flags & f) != 0; }
};

// Format specifiers.
template <typename Char>
struct basic_format_specs : align_spec, core_format_specs {
    constexpr basic_format_specs() {}
};

typedef basic_format_specs<char> format_specs;

template <typename Char, typename ErrorHandler>
constexpr unsigned basic_parse_context<Char, ErrorHandler>::next_arg_id() {
    if (next_arg_id_ >= 0) return internal::to_unsigned(next_arg_id_++);
    return 0;
}

namespace internal {

// Formats value using Grisu2 algorithm:
// https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf
template <typename Double>
FMT_API typename std::enable_if<sizeof(Double) == sizeof(uint64_t), bool>::type grisu2_format(Double value, buffer &buf, core_format_specs);
template <typename Double>
inline typename std::enable_if<sizeof(Double) != sizeof(uint64_t), bool>::type grisu2_format(Double, buffer &, core_format_specs) {
    return false;
}

template <typename Double>
void sprintf_format(Double, internal::buffer &, core_format_specs);

template <typename Handler>
constexpr void handle_int_type_spec(char spec, Handler &&handler) {
    switch (spec) {
    case 0:
    case 'd': handler.on_dec(); break;
    case 'x':
    case 'X': handler.on_hex(); break;
    case 'b':
    case 'B': handler.on_bin(); break;
    case 'o': handler.on_oct(); break;
    case 'n': handler.on_num(); break;
    default: handler.on_error();
    }
}

template <typename Handler>
constexpr void handle_float_type_spec(char spec, Handler &&handler) {
    switch (spec) {
    case 0:
    case 'g':
    case 'G': handler.on_general(); break;
    case 'e':
    case 'E': handler.on_exp(); break;
    case 'f':
    case 'F': handler.on_fixed(); break;
    case 'a':
    case 'A': handler.on_hex(); break;
    default: handler.on_error(); break;
    }
}

template <typename Char, typename Handler>
constexpr void handle_char_specs(const basic_format_specs<Char> *specs, Handler &&handler) {
    if (!specs) return handler.on_char();
    if (specs->type && specs->type != 'c') return handler.on_int();
    if (specs->align() == ALIGN_NUMERIC || specs->flags != 0) handler.on_error("invalid format specifier for char");
    handler.on_char();
}

template <typename Char, typename Handler>
constexpr void handle_cstring_type_spec(Char spec, Handler &&handler) {
    if (spec == 0 || spec == 's')
        handler.on_string();
    else if (spec == 'p')
        handler.on_pointer();
    else
        handler.on_error("invalid type specifier");
}

template <typename Char, typename ErrorHandler>
constexpr void check_string_type_spec(Char spec, ErrorHandler &&eh) {
    if (spec != 0 && spec != 's') eh.on_error("invalid type specifier");
}

template <typename Char, typename ErrorHandler>
constexpr void check_pointer_type_spec(Char spec, ErrorHandler &&eh) {
    if (spec != 0 && spec != 'p') eh.on_error("invalid type specifier");
}

template <typename ErrorHandler>
class int_type_checker : private ErrorHandler {
public:
    constexpr explicit int_type_checker(ErrorHandler eh) : ErrorHandler(eh) {}

    constexpr void on_dec() {}
    constexpr void on_hex() {}
    constexpr void on_bin() {}
    constexpr void on_oct() {}
    constexpr void on_num() {}

    constexpr void on_error() { ErrorHandler::on_error("invalid type specifier"); }
};

template <typename ErrorHandler>
class float_type_checker : private ErrorHandler {
public:
    constexpr explicit float_type_checker(ErrorHandler eh) : ErrorHandler(eh) {}

    constexpr void on_general() {}
    constexpr void on_exp() {}
    constexpr void on_fixed() {}
    constexpr void on_hex() {}

    constexpr void on_error() { ErrorHandler::on_error("invalid type specifier"); }
};

template <typename ErrorHandler>
class char_specs_checker : public ErrorHandler {
private:
    char type_;

public:
    constexpr char_specs_checker(char type, ErrorHandler eh) : ErrorHandler(eh), type_(type) {}

    constexpr void on_int() { handle_int_type_spec(type_, int_type_checker<ErrorHandler>(*this)); }
    constexpr void on_char() {}
};

template <typename ErrorHandler>
class cstring_type_checker : public ErrorHandler {
public:
    constexpr explicit cstring_type_checker(ErrorHandler eh) : ErrorHandler(eh) {}

    constexpr void on_string() {}
    constexpr void on_pointer() {}
};

template <typename Context>
void arg_map<Context>::init(const basic_format_args<Context> &args) {
    if (map_) return;
    map_ = new entry[args.max_size()];
    if (args.is_packed()) {
        for (unsigned i = 0; /*nothing*/; ++i) {
            internal::type arg_type = args.type(i);
            switch (arg_type) {
            case internal::none_type: return;
            case internal::named_arg_type: push_back(args.values_[i]); break;
            default: break;  // Do nothing.
            }
        }
    }
    for (unsigned i = 0;; ++i) {
        switch (args.args_[i].type_) {
        case internal::none_type: return;
        case internal::named_arg_type: push_back(args.args_[i].value_); break;
        default: break;  // Do nothing.
        }
    }
}

template <typename Range>
class arg_formatter_base {
public:
    typedef typename Range::value_type              char_type;
    typedef decltype(std::declval<Range>().begin()) iterator;
    typedef basic_format_specs<char_type>           format_specs;

private:
    typedef basic_writer<Range> writer_type;
    writer_type                 writer_;
    format_specs *              specs_;

    struct char_writer {
        char_type value;

        size_t size() const { return 1; }
        size_t width() const { return 1; }

        template <typename It>
        void operator()(It &&it) const {
            *it++ = value;
        }
    };

    void write_char(char_type value) {
        if (specs_)
            writer_.write_padded(*specs_, char_writer{value});
        else
            writer_.write(value);
    }

    void write_pointer(const void *p) {
        format_specs specs = specs_ ? *specs_ : format_specs();
        specs.flags        = HASH_FLAG;
        specs.type         = 'x';
        writer_.write_int(reinterpret_cast<uintptr_t>(p), specs);
    }

protected:
    writer_type & writer() { return writer_; }
    format_specs *spec() { return specs_; }
    iterator      out() { return writer_.out(); }

    void write(bool value) {
        string_view sv(value ? "true" : "false");
        specs_ ? writer_.write(sv, *specs_) : writer_.write(sv);
    }

    void write(const char_type *value) {
        if (!value) {
            string_view sv("{nullptr}");
            specs_ ? writer_.write(sv, *specs_) : writer_.write(sv);
        } else {
            auto                         length = std::char_traits<char_type>::length(value);
            basic_string_view<char_type> sv(value, length);
            specs_ ? writer_.write(sv, *specs_) : writer_.write(sv);
        }
    }

public:
    arg_formatter_base(Range r, format_specs *s, locale_ref loc) : writer_(r, loc), specs_(s) {}

    iterator operator()(monostate) {
        FMT_ASSERT(false, "invalid argument type");
        return out();
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, char_type>::value, iterator>::type operator()(T value) {
        // MSVC2013 fails to compile separate overloads for bool and char_type so
        // use std::is_same instead.
        if (std::is_same<T, bool>::value) {
            if (specs_ && specs_->type) return (*this)(value ? 1 : 0);
            write(value != 0);
        } else if (std::is_same<T, char_type>::value) {
            internal::handle_char_specs(specs_, char_spec_handler(*this, static_cast<char_type>(value)));
        } else {
            specs_ ? writer_.write_int(value, *specs_) : writer_.write(value);
        }
        return out();
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, iterator>::type operator()(T value) {
        writer_.write_double(value, specs_ ? *specs_ : format_specs());
        return out();
    }

    struct char_spec_handler : internal::error_handler {
        arg_formatter_base &formatter;
        char_type           value;

        char_spec_handler(arg_formatter_base &f, char_type val) : formatter(f), value(val) {}

        void on_int() {
            if (formatter.specs_)
                formatter.writer_.write_int(value, *formatter.specs_);
            else
                formatter.writer_.write(value);
        }
        void on_char() { formatter.write_char(value); }
    };

    struct cstring_spec_handler : internal::error_handler {
        arg_formatter_base &formatter;
        const char_type *   value;

        cstring_spec_handler(arg_formatter_base &f, const char_type *val) : formatter(f), value(val) {}

        void on_string() { formatter.write(value); }
        void on_pointer() { formatter.write_pointer(value); }
    };

    iterator operator()(const char_type *value) {
        if (!specs_) return write(value), out();
        internal::handle_cstring_type_spec(specs_->type, cstring_spec_handler(*this, value));
        return out();
    }

    iterator operator()(basic_string_view<char_type> value) {
        if (specs_) {
            internal::check_string_type_spec(specs_->type, internal::error_handler());
            writer_.write(value, *specs_);
        } else {
            writer_.write(value);
        }
        return out();
    }

    iterator operator()(const void *value) {
        if (specs_) check_pointer_type_spec(specs_->type, internal::error_handler());
        write_pointer(value);
        return out();
    }
};

template <typename Char>
constexpr bool is_name_start(Char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || '_' == c;
}

// Parses the range [begin, end) as an unsigned integer. This function assumes
// that the range is non-empty and the first character is a digit.
template <typename Char, typename ErrorHandler>
constexpr unsigned parse_nonnegative_int(const Char *&begin, const Char *end, ErrorHandler &&eh) {
    assert(begin != end && '0' <= *begin && *begin <= '9');
    if (*begin == '0') {
        ++begin;
        return 0;
    }
    unsigned value = 0;
    // Convert to unsigned to prevent a warning.
    unsigned max_int = (std::numeric_limits<int>::max)();
    unsigned big     = max_int / 10;
    do {
        // Check for overflow.
        if (value > big) {
            value = max_int + 1;
            break;
        }
        value = value * 10 + unsigned(*begin - '0');
        ++begin;
    } while (begin != end && '0' <= *begin && *begin <= '9');
    if (value > max_int) eh.on_error("number is too big");
    return value;
}

template <typename Char, typename Context>
class custom_formatter : public function<bool> {
private:
    Context &ctx_;

public:
    explicit custom_formatter(Context &ctx) : ctx_(ctx) {}

    bool operator()(typename basic_format_arg<Context>::handle h) const {
        h.format(ctx_);
        return true;
    }

    template <typename T>
    bool operator()(T) const {
        return false;
    }
};

template <typename T>
struct is_integer {
    enum { value = std::is_integral<T>::value && !std::is_same<T, bool>::value && !std::is_same<T, char>::value && !std::is_same<T, wchar_t>::value };
};

template <typename ErrorHandler>
class width_checker : public function<unsigned long long> {
public:
    explicit constexpr width_checker(ErrorHandler &eh) : handler_(eh) {}

    template <typename T>
    constexpr typename std::enable_if<is_integer<T>::value, unsigned long long>::type operator()(T value) {
        if (is_negative(value)) handler_.on_error("negative width");
        return static_cast<unsigned long long>(value);
    }

    template <typename T>
    constexpr typename std::enable_if<!is_integer<T>::value, unsigned long long>::type operator()(T) {
        handler_.on_error("width is not integer");
        return 0;
    }

private:
    ErrorHandler &handler_;
};

template <typename ErrorHandler>
class precision_checker : public function<unsigned long long> {
public:
    explicit constexpr precision_checker(ErrorHandler &eh) : handler_(eh) {}

    template <typename T>
    constexpr typename std::enable_if<is_integer<T>::value, unsigned long long>::type operator()(T value) {
        if (is_negative(value)) handler_.on_error("negative precision");
        return static_cast<unsigned long long>(value);
    }

    template <typename T>
    constexpr typename std::enable_if<!is_integer<T>::value, unsigned long long>::type operator()(T) {
        handler_.on_error("precision is not integer");
        return 0;
    }

private:
    ErrorHandler &handler_;
};

// A format specifier handler that sets fields in basic_format_specs.
template <typename Char>
class specs_setter {
public:
    explicit constexpr specs_setter(basic_format_specs<Char> &specs) : specs_(specs) {}

    constexpr specs_setter(const specs_setter &other) : specs_(other.specs_) {}

    constexpr void on_align(alignment align) { specs_.align_ = align; }
    constexpr void on_fill(Char fill) { specs_.fill_ = fill; }
    constexpr void on_plus() { specs_.flags |= SIGN_FLAG | PLUS_FLAG; }
    constexpr void on_minus() { specs_.flags |= MINUS_FLAG; }
    constexpr void on_space() { specs_.flags |= SIGN_FLAG; }
    constexpr void on_hash() { specs_.flags |= HASH_FLAG; }

    constexpr void on_zero() {
        specs_.align_ = ALIGN_NUMERIC;
        specs_.fill_  = '0';
    }

    constexpr void on_width(unsigned width) { specs_.width_ = width; }
    constexpr void on_precision(unsigned precision) { specs_.precision = static_cast<int>(precision); }
    constexpr void end_precision() {}

    constexpr void on_type(Char type) { specs_.type = static_cast<char>(type); }

protected:
    basic_format_specs<Char> &specs_;
};

// A format specifier handler that checks if specifiers are consistent with the
// argument type.
template <typename Handler>
class specs_checker : public Handler {
public:
    constexpr specs_checker(const Handler &handler, internal::type arg_type) : Handler(handler), arg_type_(arg_type) {}

    constexpr specs_checker(const specs_checker &other) : Handler(other), arg_type_(other.arg_type_) {}

    constexpr void on_align(alignment align) {
        if (align == ALIGN_NUMERIC) require_numeric_argument();
        Handler::on_align(align);
    }

    constexpr void on_plus() {
        check_sign();
        Handler::on_plus();
    }

    constexpr void on_minus() {
        check_sign();
        Handler::on_minus();
    }

    constexpr void on_space() {
        check_sign();
        Handler::on_space();
    }

    constexpr void on_hash() {
        require_numeric_argument();
        Handler::on_hash();
    }

    constexpr void on_zero() {
        require_numeric_argument();
        Handler::on_zero();
    }

    constexpr void end_precision() {
        if (is_integral(arg_type_) || arg_type_ == pointer_type) this->on_error("precision not allowed for this argument type");
    }

private:
    constexpr void require_numeric_argument() {
        if (!is_arithmetic(arg_type_)) this->on_error("format specifier requires numeric argument");
    }

    constexpr void check_sign() {
        require_numeric_argument();
        if (is_integral(arg_type_) && arg_type_ != int_type && arg_type_ != long_long_type && arg_type_ != internal::char_type) {
            this->on_error("format specifier requires signed argument");
        }
    }

    internal::type arg_type_;
};

template <template <typename> class Handler, typename T, typename Context, typename ErrorHandler>
constexpr void set_dynamic_spec(T &value, basic_format_arg<Context> arg, ErrorHandler eh) {
    unsigned long long big_value = visit_format_arg(Handler<ErrorHandler>(eh), arg);
    if (big_value > to_unsigned((std::numeric_limits<int>::max)())) eh.on_error("number is too big");
    value = static_cast<T>(big_value);
}

struct auto_id {};

// The standard format specifier handler with checking.
template <typename Context>
class specs_handler : public specs_setter<typename Context::char_type> {
public:
    typedef typename Context::char_type char_type;

    constexpr specs_handler(basic_format_specs<char_type> &specs, Context &ctx) : specs_setter<char_type>(specs), context_(ctx) {}

    template <typename Id>
    constexpr void on_dynamic_width(Id arg_id) {
        set_dynamic_spec<width_checker>(this->specs_.width_, get_arg(arg_id), context_.error_handler());
    }

    template <typename Id>
    constexpr void on_dynamic_precision(Id arg_id) {
        set_dynamic_spec<precision_checker>(this->specs_.precision, get_arg(arg_id), context_.error_handler());
    }

    void on_error(const char *message) { context_.on_error(message); }

private:
    constexpr basic_format_arg<Context> get_arg(auto_id) { return context_.next_arg(); }

    template <typename Id>
    constexpr basic_format_arg<Context> get_arg(Id arg_id) {
        context_.parse_context().check_arg_id(arg_id);
        return context_.get_arg(arg_id);
    }

    Context &context_;
};

// An argument reference.
template <typename Char>
struct arg_ref {
    enum Kind { NONE, INDEX, NAME };

    constexpr arg_ref() : kind(NONE), index(0) {}
    constexpr explicit arg_ref(unsigned index) : kind(INDEX), index(index) {}
    explicit arg_ref(basic_string_view<Char> nm) : kind(NAME) { name = {nm.data(), nm.size()}; }

    constexpr arg_ref &operator=(unsigned idx) {
        kind  = INDEX;
        index = idx;
        return *this;
    }

    Kind kind;
    union {
        unsigned           index;
        string_value<Char> name;  // This is not string_view because of gcc 4.4.
    };
};

// Format specifiers with width and precision resolved at formatting rather
// than parsing time to allow re-using the same parsed specifiers with
// differents sets of arguments (precompilation of format strings).
template <typename Char>
struct dynamic_format_specs : basic_format_specs<Char> {
    arg_ref<Char> width_ref;
    arg_ref<Char> precision_ref;
};

// Format spec handler that saves references to arguments representing dynamic
// width and precision to be resolved at formatting time.
template <typename ParseContext>
class dynamic_specs_handler : public specs_setter<typename ParseContext::char_type> {
public:
    typedef typename ParseContext::char_type char_type;

    constexpr dynamic_specs_handler(dynamic_format_specs<char_type> &specs, ParseContext &ctx) : specs_setter<char_type>(specs), specs_(specs), context_(ctx) {}

    constexpr dynamic_specs_handler(const dynamic_specs_handler &other) : specs_setter<char_type>(other), specs_(other.specs_), context_(other.context_) {}

    template <typename Id>
    constexpr void on_dynamic_width(Id arg_id) {
        specs_.width_ref = make_arg_ref(arg_id);
    }

    template <typename Id>
    constexpr void on_dynamic_precision(Id arg_id) {
        specs_.precision_ref = make_arg_ref(arg_id);
    }

    constexpr void on_error(const char *message) { context_.on_error(message); }

private:
    typedef arg_ref<char_type> arg_ref_type;

    template <typename Id>
    constexpr arg_ref_type make_arg_ref(Id arg_id) {
        context_.check_arg_id(arg_id);
        return arg_ref_type(arg_id);
    }

    constexpr arg_ref_type make_arg_ref(auto_id) { return arg_ref_type(context_.next_arg_id()); }

    dynamic_format_specs<char_type> &specs_;
    ParseContext &                   context_;
};

template <typename Char, typename IDHandler>
constexpr const Char *parse_arg_id(const Char *begin, const Char *end, IDHandler &&handler) {
    assert(begin != end);
    Char c = *begin;
    if (c == '}' || c == ':') return handler(), begin;
    if (c >= '0' && c <= '9') {
        unsigned index = parse_nonnegative_int(begin, end, handler);
        if (begin == end || (*begin != '}' && *begin != ':')) return handler.on_error("invalid format string"), begin;
        handler(index);
        return begin;
    }
    if (!is_name_start(c)) return handler.on_error("invalid format string"), begin;
    auto it = begin;
    do { ++it; } while (it != end && (is_name_start(c = *it) || ('0' <= c && c <= '9')));
    handler(basic_string_view<Char>(begin, to_unsigned(it - begin)));
    return it;
}

// Adapts SpecHandler to IDHandler API for dynamic width.
template <typename SpecHandler, typename Char>
struct width_adapter {
    explicit constexpr width_adapter(SpecHandler &h) : handler(h) {}

    constexpr void operator()() { handler.on_dynamic_width(auto_id()); }
    constexpr void operator()(unsigned id) { handler.on_dynamic_width(id); }
    constexpr void operator()(basic_string_view<Char> id) { handler.on_dynamic_width(id); }

    constexpr void on_error(const char *message) { handler.on_error(message); }

    SpecHandler &handler;
};

// Adapts SpecHandler to IDHandler API for dynamic precision.
template <typename SpecHandler, typename Char>
struct precision_adapter {
    explicit constexpr precision_adapter(SpecHandler &h) : handler(h) {}

    constexpr void operator()() { handler.on_dynamic_precision(auto_id()); }
    constexpr void operator()(unsigned id) { handler.on_dynamic_precision(id); }
    constexpr void operator()(basic_string_view<Char> id) { handler.on_dynamic_precision(id); }

    constexpr void on_error(const char *message) { handler.on_error(message); }

    SpecHandler &handler;
};

// Parses fill and alignment.
template <typename Char, typename Handler>
constexpr const Char *parse_align(const Char *begin, const Char *end, Handler &&handler) {
    FMT_ASSERT(begin != end, "");
    alignment align = ALIGN_DEFAULT;
    int       i     = 0;
    if (begin + 1 != end) ++i;
    do {
        switch (static_cast<char>(begin[i])) {
        case '<': align = ALIGN_LEFT; break;
        case '>': align = ALIGN_RIGHT; break;
        case '=': align = ALIGN_NUMERIC; break;
        case '^': align = ALIGN_CENTER; break;
        }
        if (align != ALIGN_DEFAULT) {
            if (i > 0) {
                auto c = *begin;
                if (c == '{') return handler.on_error("invalid fill character '{'"), begin;
                begin += 2;
                handler.on_fill(c);
            } else
                ++begin;
            handler.on_align(align);
            break;
        }
    } while (i-- > 0);
    return begin;
}

template <typename Char, typename Handler>
constexpr const Char *parse_width(const Char *begin, const Char *end, Handler &&handler) {
    FMT_ASSERT(begin != end, "");
    if ('0' <= *begin && *begin <= '9') {
        handler.on_width(parse_nonnegative_int(begin, end, handler));
    } else if (*begin == '{') {
        ++begin;
        if (begin != end) begin = parse_arg_id(begin, end, width_adapter<Handler, Char>(handler));
        if (begin == end || *begin != '}') return handler.on_error("invalid format string"), begin;
        ++begin;
    }
    return begin;
}

// Parses standard format specifiers and sends notifications about parsed
// components to handler.
template <typename Char, typename SpecHandler>
constexpr const Char *parse_format_specs(const Char *begin, const Char *end, SpecHandler &&handler) {
    if (begin == end || *begin == '}') return begin;

    begin = parse_align(begin, end, handler);
    if (begin == end) return begin;

    // Parse sign.
    switch (static_cast<char>(*begin)) {
    case '+':
        handler.on_plus();
        ++begin;
        break;
    case '-':
        handler.on_minus();
        ++begin;
        break;
    case ' ':
        handler.on_space();
        ++begin;
        break;
    }
    if (begin == end) return begin;

    if (*begin == '#') {
        handler.on_hash();
        if (++begin == end) return begin;
    }

    // Parse zero flag.
    if (*begin == '0') {
        handler.on_zero();
        if (++begin == end) return begin;
    }

    begin = parse_width(begin, end, handler);
    if (begin == end) return begin;

    // Parse precision.
    if (*begin == '.') {
        ++begin;
        auto c = begin != end ? *begin : 0;
        if ('0' <= c && c <= '9') {
            handler.on_precision(parse_nonnegative_int(begin, end, handler));
        } else if (c == '{') {
            ++begin;
            if (begin != end) { begin = parse_arg_id(begin, end, precision_adapter<SpecHandler, Char>(handler)); }
            if (begin == end || *begin++ != '}') return handler.on_error("invalid format string"), begin;
        } else {
            return handler.on_error("missing precision specifier"), begin;
        }
        handler.end_precision();
    }

    // Parse type.
    if (begin != end && *begin != '}') handler.on_type(*begin++);
    return begin;
}

// Return the result via the out param to workaround gcc bug 77539.
template <bool IS_CONSTEXPR, typename T, typename Ptr = const T *>
constexpr bool find(Ptr first, Ptr last, T value, Ptr &out) {
    for (out = first; out != last; ++out) {
        if (*out == value) return true;
    }
    return false;
}

template <>
inline bool find<false, char>(const char *first, const char *last, char value, const char *&out) {
    out = static_cast<const char *>(std::memchr(first, value, internal::to_unsigned(last - first)));
    return out != nullptr;
}

template <typename Handler, typename Char>
struct id_adapter {
    constexpr void operator()() { handler.on_arg_id(); }
    constexpr void operator()(unsigned id) { handler.on_arg_id(id); }
    constexpr void operator()(basic_string_view<Char> id) { handler.on_arg_id(id); }
    constexpr void on_error(const char *message) { handler.on_error(message); }
    Handler &      handler;
};

template <bool IS_CONSTEXPR, typename Char, typename Handler>
constexpr void parse_format_string(basic_string_view<Char> format_str, Handler &&handler) {
    struct writer {
        constexpr void operator()(const Char *begin, const Char *end) {
            if (begin == end) return;
            for (;;) {
                const Char *p = nullptr;
                if (!find<IS_CONSTEXPR>(begin, end, '}', p)) return handler_.on_text(begin, end);
                ++p;
                if (p == end || *p != '}') return handler_.on_error("unmatched '}' in format string");
                handler_.on_text(begin, p);
                begin = p + 1;
            }
        }
        Handler &handler_;
    } write{handler};
    auto begin = format_str.data();
    auto end   = begin + format_str.size();
    while (begin != end) {
        // Doing two passes with memchr (one for '{' and another for '}') is up to
        // 2.5x faster than the naive one-pass implementation on big format strings.
        const Char *p = begin;
        if (*begin != '{' && !find<IS_CONSTEXPR>(begin, end, '{', p)) return write(begin, end);
        write(begin, p);
        ++p;
        if (p == end) return handler.on_error("invalid format string");
        if (static_cast<char>(*p) == '}') {
            handler.on_arg_id();
            handler.on_replacement_field(p);
        } else if (*p == '{') {
            handler.on_text(p, p + 1);
        } else {
            p      = parse_arg_id(p, end, id_adapter<Handler, Char>{handler});
            Char c = p != end ? *p : Char();
            if (c == '}') {
                handler.on_replacement_field(p);
            } else if (c == ':') {
                p = handler.on_format_specs(p + 1, end);
                if (p == end || *p != '}') return handler.on_error("unknown format specifier");
            } else {
                return handler.on_error("missing '}' in format string");
            }
        }
        begin = p + 1;
    }
}

template <typename T, typename ParseContext>
constexpr const typename ParseContext::char_type *parse_format_specs(ParseContext &ctx) {
    // GCC 7.2 requires initializer.
    formatter<T, typename ParseContext::char_type> f{};
    return f.parse(ctx);
}

template <typename Char, typename ErrorHandler, typename... Args>
class format_string_checker {
public:
    explicit constexpr format_string_checker(basic_string_view<Char> format_str, ErrorHandler eh)
        : arg_id_((std::numeric_limits<unsigned>::max)()), context_(format_str, eh), parse_funcs_{&parse_format_specs<Args, parse_context_type>...} {}

    constexpr void on_text(const Char *, const Char *) {}

    constexpr void on_arg_id() {
        arg_id_ = context_.next_arg_id();
        check_arg_id();
    }
    constexpr void on_arg_id(unsigned id) {
        arg_id_ = id;
        context_.check_arg_id(id);
        check_arg_id();
    }
    constexpr void on_arg_id(basic_string_view<Char>) {}

    constexpr void on_replacement_field(const Char *) {}

    constexpr const Char *on_format_specs(const Char *begin, const Char *) {
        context_.advance_to(begin);
        return arg_id_ < NUM_ARGS ? parse_funcs_[arg_id_](context_) : begin;
    }

    constexpr void on_error(const char *message) { context_.on_error(message); }

private:
    typedef basic_parse_context<Char, ErrorHandler> parse_context_type;
    enum { NUM_ARGS = sizeof...(Args) };

    constexpr void check_arg_id() {
        if (arg_id_ >= NUM_ARGS) context_.on_error("argument index out of range");
    }

    // Format specifier parsing function.
    typedef const Char *(*parse_func)(parse_context_type &);

    unsigned           arg_id_;
    parse_context_type context_;
    parse_func         parse_funcs_[NUM_ARGS > 0 ? NUM_ARGS : 1];
};

template <typename Char, typename ErrorHandler, typename... Args>
constexpr bool do_check_format_string(basic_string_view<Char> s, ErrorHandler eh = ErrorHandler()) {
    format_string_checker<Char, ErrorHandler, Args...> checker(s, eh);
    parse_format_string<true>(s, checker);
    return true;
}

template <typename... Args, typename S>
typename std::enable_if<is_compile_string<S>::value>::type check_format_string(S format_str) {
    typedef typename S::char_type char_t;
    constexpr bool                invalid_format = internal::do_check_format_string<char_t, internal::error_handler, Args...>(to_string_view(format_str));
    (void)invalid_format;
}

// Specifies whether to format T using the standard formatter.
// It is not possible to use get_type in formatter specialization directly
// because of a bug in MSVC.
template <typename Context, typename T>
struct format_type : std::integral_constant<bool, get_type<Context, T>::value != custom_type> {};

template <template <typename> class Handler, typename Spec, typename Context>
void handle_dynamic_spec(Spec &value, arg_ref<typename Context::char_type> ref, Context &ctx) {
    typedef typename Context::char_type char_type;
    switch (ref.kind) {
    case arg_ref<char_type>::NONE: break;
    case arg_ref<char_type>::INDEX: internal::set_dynamic_spec<Handler>(value, ctx.get_arg(ref.index), ctx.error_handler()); break;
    case arg_ref<char_type>::NAME: internal::set_dynamic_spec<Handler>(value, ctx.get_arg({ref.name.value, ref.name.size}), ctx.error_handler()); break;
    }
}
}  // namespace internal

/** The default argument formatter. */
template <typename Range>
class arg_formatter : public internal::function<typename internal::arg_formatter_base<Range>::iterator>, public internal::arg_formatter_base<Range> {
private:
    typedef typename Range::value_type                               char_type;
    typedef internal::arg_formatter_base<Range>                      base;
    typedef basic_format_context<typename base::iterator, char_type> context_type;

    context_type &ctx_;

public:
    typedef Range                       range;
    typedef typename base::iterator     iterator;
    typedef typename base::format_specs format_specs;

    /**
      \rst
      Constructs an argument formatter object.
      *ctx* is a reference to the formatting context,
      *spec* contains format specifier information for standard argument types.
      \endrst
     */
    explicit arg_formatter(context_type &ctx, format_specs *spec = nullptr) : base(Range(ctx.out()), spec, ctx.locale()), ctx_(ctx) {}

    // Deprecated.
    arg_formatter(context_type &ctx, format_specs &spec) : base(Range(ctx.out()), &spec), ctx_(ctx) {}

    using base::operator();

    /** Formats an argument of a user-defined type. */
    iterator operator()(typename basic_format_arg<context_type>::handle handle) {
        handle.format(ctx_);
        return this->out();
    }
};

/**
  This template provides operations for formatting and writing data into a
  character range.
 */
template <typename Range>
class basic_writer {
public:
    typedef typename Range::value_type              char_type;
    typedef decltype(std::declval<Range>().begin()) iterator;
    typedef basic_format_specs<char_type>           format_specs;

private:
    iterator             out_;  // Output iterator.
    internal::locale_ref locale_;

    // Attempts to reserve space for n extra characters in the output range.
    // Returns a pointer to the reserved range or a reference to out_.
    auto reserve(std::size_t n) -> decltype(internal::reserve(out_, n)) { return internal::reserve(out_, n); }

    // Writes a value in the format
    //   <left-padding><value><right-padding>
    // where <value> is written by f(it).
    template <typename F>
    void write_padded(const align_spec &spec, F &&f) {
        unsigned width           = spec.width();  // User-perceived width (in code points).
        size_t   size            = f.size();      // The number of code units.
        size_t   num_code_points = width != 0 ? f.width() : size;
        if (width <= num_code_points) return f(reserve(size));
        auto &&     it      = reserve(width + (size - num_code_points));
        char_type   fill    = static_cast<char_type>(spec.fill());
        std::size_t padding = width - num_code_points;
        if (spec.align() == ALIGN_RIGHT) {
            it = std::fill_n(it, padding, fill);
            f(it);
        } else if (spec.align() == ALIGN_CENTER) {
            std::size_t left_padding = padding / 2;
            it                       = std::fill_n(it, left_padding, fill);
            f(it);
            it = std::fill_n(it, padding - left_padding, fill);
        } else {
            f(it);
            it = std::fill_n(it, padding, fill);
        }
    }

    template <typename F>
    struct padded_int_writer {
        size_t      size_;
        string_view prefix;
        char_type   fill;
        std::size_t padding;
        F           f;

        size_t size() const { return size_; }
        size_t width() const { return size_; }

        template <typename It>
        void operator()(It &&it) const {
            if (prefix.size() != 0) it = internal::copy_str<char_type>(prefix.begin(), prefix.end(), it);
            it = std::fill_n(it, padding, fill);
            f(it);
        }
    };

    // Writes an integer in the format
    //   <left-padding><prefix><numeric-padding><digits><right-padding>
    // where <digits> are written by f(it).
    template <typename Spec, typename F>
    void write_int(int num_digits, string_view prefix, const Spec &spec, F f) {
        std::size_t size    = prefix.size() + internal::to_unsigned(num_digits);
        char_type   fill    = static_cast<char_type>(spec.fill());
        std::size_t padding = 0;
        if (spec.align() == ALIGN_NUMERIC) {
            if (spec.width() > size) {
                padding = spec.width() - size;
                size    = spec.width();
            }
        } else if (spec.precision > num_digits) {
            size    = prefix.size() + internal::to_unsigned(spec.precision);
            padding = internal::to_unsigned(spec.precision - num_digits);
            fill    = static_cast<char_type>('0');
        }
        align_spec as = spec;
        if (spec.align() == ALIGN_DEFAULT) as.align_ = ALIGN_RIGHT;
        write_padded(as, padded_int_writer<F>{size, prefix, fill, padding, f});
    }

    // Writes a decimal integer.
    template <typename Int>
    void write_decimal(Int value) {
        typedef typename internal::int_traits<Int>::main_type main_type;
        main_type                                             abs_value   = static_cast<main_type>(value);
        bool                                                  is_negative = internal::is_negative(value);
        if (is_negative) abs_value = 0 - abs_value;
        int    num_digits = internal::count_digits(abs_value);
        auto &&it         = reserve((is_negative ? 1 : 0) + static_cast<size_t>(num_digits));
        if (is_negative) *it++ = static_cast<char_type>('-');
        it = internal::format_decimal<char_type>(it, abs_value, num_digits);
    }

    // The handle_int_type_spec handler that writes an integer.
    template <typename Int, typename Spec>
    struct int_writer {
        typedef typename internal::int_traits<Int>::main_type unsigned_type;

        basic_writer<Range> &writer;
        const Spec &         spec;
        unsigned_type        abs_value;
        char                 prefix[4];
        unsigned             prefix_size;

        string_view get_prefix() const { return string_view(prefix, prefix_size); }

        // Counts the number of digits in abs_value. BITS = log2(radix).
        template <unsigned BITS>
        int count_digits() const {
            unsigned_type n          = abs_value;
            int           num_digits = 0;
            do { ++num_digits; } while ((n >>= BITS) != 0);
            return num_digits;
        }

        int_writer(basic_writer<Range> &w, Int value, const Spec &s) : writer(w), spec(s), abs_value(static_cast<unsigned_type>(value)), prefix_size(0) {
            if (internal::is_negative(value)) {
                prefix[0] = '-';
                ++prefix_size;
                abs_value = 0 - abs_value;
            } else if (spec.has(SIGN_FLAG)) {
                prefix[0] = spec.has(PLUS_FLAG) ? '+' : ' ';
                ++prefix_size;
            }
        }

        struct dec_writer {
            unsigned_type abs_value;
            int           num_digits;

            template <typename It>
            void operator()(It &&it) const {
                it = internal::format_decimal<char_type>(it, abs_value, num_digits);
            }
        };

        void on_dec() {
            int num_digits = internal::count_digits(abs_value);
            writer.write_int(num_digits, get_prefix(), spec, dec_writer{abs_value, num_digits});
        }

        struct hex_writer {
            int_writer &self;
            int         num_digits;

            template <typename It>
            void operator()(It &&it) const {
                it = internal::format_uint<4, char_type>(it, self.abs_value, num_digits, self.spec.type != 'x');
            }
        };

        void on_hex() {
            if (spec.has(HASH_FLAG)) {
                prefix[prefix_size++] = '0';
                prefix[prefix_size++] = static_cast<char>(spec.type);
            }
            int num_digits = count_digits<4>();
            writer.write_int(num_digits, get_prefix(), spec, hex_writer{*this, num_digits});
        }

        template <int BITS>
        struct bin_writer {
            unsigned_type abs_value;
            int           num_digits;

            template <typename It>
            void operator()(It &&it) const {
                it = internal::format_uint<BITS, char_type>(it, abs_value, num_digits);
            }
        };

        void on_bin() {
            if (spec.has(HASH_FLAG)) {
                prefix[prefix_size++] = '0';
                prefix[prefix_size++] = static_cast<char>(spec.type);
            }
            int num_digits = count_digits<1>();
            writer.write_int(num_digits, get_prefix(), spec, bin_writer<1>{abs_value, num_digits});
        }

        void on_oct() {
            int num_digits = count_digits<3>();
            if (spec.has(HASH_FLAG) && spec.precision <= num_digits) {
                // Octal prefix '0' is counted as a digit, so only add it if precision
                // is not greater than the number of digits.
                prefix[prefix_size++] = '0';
            }
            writer.write_int(num_digits, get_prefix(), spec, bin_writer<3>{abs_value, num_digits});
        }

        enum { SEP_SIZE = 1 };

        struct num_writer {
            unsigned_type abs_value;
            int           size;
            char_type     sep;

            template <typename It>
            void operator()(It &&it) const {
                basic_string_view<char_type> s(&sep, SEP_SIZE);
                it = internal::format_decimal<char_type>(it, abs_value, size, internal::add_thousands_sep<char_type>(s));
            }
        };

        void on_num() {
            int       num_digits = internal::count_digits(abs_value);
            char_type sep        = internal::thousands_sep<char_type>(writer.locale_);
            int       size       = num_digits + SEP_SIZE * ((num_digits - 1) / 3);
            writer.write_int(size, get_prefix(), spec, num_writer{abs_value, size, sep});
        }

        void on_error() { FMT_THROW("invalid type specifier"); }
    };

    // Writes a formatted integer.
    template <typename T, typename Spec>
    void write_int(T value, const Spec &spec) {
        internal::handle_int_type_spec(spec.type, int_writer<T, Spec>(*this, value, spec));
    }

    enum { INF_SIZE = 3 };  // This is an enum to workaround a bug in MSVC.

    struct inf_or_nan_writer {
        char        sign;
        const char *str;

        size_t size() const { return static_cast<std::size_t>(INF_SIZE + (sign ? 1 : 0)); }
        size_t width() const { return size(); }

        template <typename It>
        void operator()(It &&it) const {
            if (sign) *it++ = static_cast<char_type>(sign);
            it = internal::copy_str<char_type>(str, str + static_cast<std::size_t>(INF_SIZE), it);
        }
    };

    struct double_writer {
        size_t            n;
        char              sign;
        internal::buffer &buffer;

        size_t size() const { return buffer.size() + (sign ? 1 : 0); }
        size_t width() const { return size(); }

        template <typename It>
        void operator()(It &&it) {
            if (sign) {
                *it++ = static_cast<char_type>(sign);
                --n;
            }
            it = internal::copy_str<char_type>(buffer.begin(), buffer.end(), it);
        }
    };

    // Formats a floating-point number (double or long double).
    template <typename T>
    void write_double(T value, const format_specs &spec);

    template <typename Char>
    struct str_writer {
        const Char *s;
        size_t      size_;

        size_t size() const { return size_; }
        size_t width() const { return internal::count_code_points(basic_string_view<Char>(s, size_)); }

        template <typename It>
        void operator()(It &&it) const {
            it = internal::copy_str<char_type>(s, s + size_, it);
        }
    };

    template <typename Char>
    friend class internal::arg_formatter_base;

public:
    /** Constructs a ``basic_writer`` object. */
    explicit basic_writer(Range out, internal::locale_ref loc = internal::locale_ref()) : out_(out.begin()), locale_(loc) {}

    iterator out() const { return out_; }

    void write(int value) { write_decimal(value); }
    void write(long value) { write_decimal(value); }
    void write(long long value) { write_decimal(value); }

    void write(unsigned value) { write_decimal(value); }
    void write(unsigned long value) { write_decimal(value); }
    void write(unsigned long long value) { write_decimal(value); }

    /**
      \rst
      Formats *value* and writes it to the buffer.
      \endrst
     */
    template <typename T, typename FormatSpec, typename... FormatSpecs>
    typename std::enable_if<std::is_integral<T>::value, void>::type write(T value, FormatSpec spec, FormatSpecs... specs) {
        format_specs s(spec, specs...);
        s.align_ = ALIGN_RIGHT;
        write_int(value, s);
    }

    void write(double value) { write_double(value, format_specs()); }

    /**
      \rst
      Formats *value* using the general format for floating-point numbers
      (``'g'``) and writes it to the buffer.
      \endrst
     */
    void write(long double value) { write_double(value, format_specs()); }

    /** Writes a character to the buffer. */
    void write(char value) { *reserve(1) = value; }
    void write(wchar_t value) {
        static_assert(std::is_same<char_type, wchar_t>::value, "");
        *reserve(1) = value;
    }

    /**
      \rst
      Writes *value* to the buffer.
      \endrst
     */
    void write(string_view value) {
        auto &&it = reserve(value.size());
        it        = internal::copy_str<char_type>(value.begin(), value.end(), it);
    }
    void write(wstring_view value) {
        static_assert(std::is_same<char_type, wchar_t>::value, "");
        auto &&it = reserve(value.size());
        it        = std::copy(value.begin(), value.end(), it);
    }

    // Writes a formatted string.
    template <typename Char>
    void write(const Char *s, std::size_t size, const align_spec &spec) {
        write_padded(spec, str_writer<Char>{s, size});
    }

    template <typename Char>
    void write(basic_string_view<Char> s, const format_specs &spec = format_specs()) {
        const Char *data = s.data();
        std::size_t size = s.size();
        if (spec.precision >= 0 && internal::to_unsigned(spec.precision) < size) size = internal::to_unsigned(spec.precision);
        write(data, size, spec);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, void>::value>::type write(const T *p) {
        format_specs specs;
        specs.flags = HASH_FLAG;
        specs.type  = 'x';
        write_int(reinterpret_cast<uintptr_t>(p), specs);
    }
};

struct float_spec_handler {
    char type;
    bool upper;

    explicit float_spec_handler(char t) : type(t), upper(false) {}

    void on_general() {
        if (type == 'G')
            upper = true;
        else
            type = 'g';
    }

    void on_exp() {
        if (type == 'E') upper = true;
    }

    void on_fixed() {
        if (type == 'F') {
            upper = true;
#if FMT_MSC_VER
            // MSVC's printf doesn't support 'F'.
            type = 'f';
#endif
        }
    }

    void on_hex() {
        if (type == 'A') upper = true;
    }

    [[noreturn]] void on_error() { FMT_THROW("invalid type specifier"); }
};

template <typename Range>
template <typename T>
void basic_writer<Range>::write_double(T value, const format_specs &spec) {
    // Check type.
    float_spec_handler handler(static_cast<char>(spec.type));
    internal::handle_float_type_spec(handler.type, handler);

    char sign = 0;
    // Use signbit instead of value < 0 because the latter is always
    // false for NaN.
    if (std::signbit(value)) {
        sign  = '-';
        value = -value;
    } else if (spec.has(SIGN_FLAG)) {
        sign = spec.has(PLUS_FLAG) ? '+' : ' ';
    }

    struct write_inf_or_nan_t {
        basic_writer &writer;
        format_specs  spec;
        char          sign;
        void          operator()(const char *str) const { writer.write_padded(spec, inf_or_nan_writer{sign, str}); }
    } write_inf_or_nan = {*this, spec, sign};

    // Format NaN and ininity ourselves because sprintf's output is not consistent
    // across platforms.
    if (internal::fputil::isnotanumber(value)) return write_inf_or_nan(handler.upper ? "NAN" : "nan");
    if (internal::fputil::isinfinity(value)) return write_inf_or_nan(handler.upper ? "INF" : "inf");

    memory_buffer buffer;
    bool          use_grisu = FMT_USE_GRISU && sizeof(T) <= sizeof(double) && spec.type != 'a' && spec.type != 'A'
                     && internal::grisu2_format(static_cast<double>(value), buffer, spec);
    if (!use_grisu) {
        format_specs normalized_spec(spec);
        normalized_spec.type = handler.type;
        internal::sprintf_format(value, buffer, normalized_spec);
    }
    size_t     n  = buffer.size();
    align_spec as = spec;
    if (spec.align() == ALIGN_NUMERIC) {
        if (sign) {
            auto &&it = reserve(1);
            *it++     = static_cast<char_type>(sign);
            sign      = 0;
            if (as.width_) --as.width_;
        }
        as.align_ = ALIGN_RIGHT;
    } else {
        if (spec.align() == ALIGN_DEFAULT) as.align_ = ALIGN_RIGHT;
        if (sign) ++n;
    }
    write_padded(as, double_writer{n, sign, buffer});
}

// Reports a system error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_system_error(int error_code, string_view message) noexcept;

#if FMT_USE_WINDOWS_H

/** A Windows error. */
class windows_error : public system_error {
private:
    FMT_API void init(int error_code, string_view format_str, format_args args);

public:
    /**
     \rst
     Constructs a :class:`fmt::windows_error` object with the description
     of the form

     .. parsed-literal::
       *<message>*: *<system-message>*

     where *<message>* is the formatted message and *<system-message>* is the
     system message corresponding to the error code.
     *error_code* is a Windows error code as given by ``GetLastError``.
     If *error_code* is not a valid error code such as -1, the system message
     will look like "error -1".

     **Example**::

       // This throws a windows_error with the description
       //   cannot open file 'madeup': The system cannot find the file specified.
       // or similar (system message may vary).
       const char *filename = "madeup";
       LPOFSTRUCT of = LPOFSTRUCT();
       HFILE file = OpenFile(filename, &of, OF_READ);
       if (file == HFILE_ERROR) {
         throw fmt::windows_error(GetLastError(),
                                  "cannot open file '{}'", filename);
       }
     \endrst
    */
    template <typename... Args>
    windows_error(int error_code, string_view message, const Args &... args) {
        init(error_code, message, make_format_args(args...));
    }
};

// Reports a Windows error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_windows_error(int error_code, string_view message) noexcept;

#endif

/** Fast integer formatter. */
class format_int {
private:
    // Buffer should be large enough to hold all digits (digits10 + 1),
    // a sign and a null character.
    enum { BUFFER_SIZE = std::numeric_limits<unsigned long long>::digits10 + 3 };
    mutable char buffer_[BUFFER_SIZE];
    char *       str_;

    // Formats value in reverse and returns a pointer to the beginning.
    char *format_decimal(unsigned long long value) {
        char *ptr = buffer_ + (BUFFER_SIZE - 1);  // Parens to workaround MSVC bug.
        while (value >= 100) {
            // Integer division is slow so do it for a group of two digits instead
            // of for every digit. The idea comes from the talk by Alexandrescu
            // "Three Optimization Tips for C++". See speed-test for a comparison.
            unsigned index = static_cast<unsigned>((value % 100) * 2);
            value /= 100;
            *--ptr = internal::data::DIGITS[index + 1];
            *--ptr = internal::data::DIGITS[index];
        }
        if (value < 10) {
            *--ptr = static_cast<char>('0' + value);
            return ptr;
        }
        unsigned index = static_cast<unsigned>(value * 2);
        *--ptr         = internal::data::DIGITS[index + 1];
        *--ptr         = internal::data::DIGITS[index];
        return ptr;
    }

    void format_signed(long long value) {
        unsigned long long abs_value = static_cast<unsigned long long>(value);
        bool               negative  = value < 0;
        if (negative) abs_value = 0 - abs_value;
        str_ = format_decimal(abs_value);
        if (negative) *--str_ = '-';
    }

public:
    explicit format_int(int value) { format_signed(value); }
    explicit format_int(long value) { format_signed(value); }
    explicit format_int(long long value) { format_signed(value); }
    explicit format_int(unsigned value) : str_(format_decimal(value)) {}
    explicit format_int(unsigned long value) : str_(format_decimal(value)) {}
    explicit format_int(unsigned long long value) : str_(format_decimal(value)) {}

    /** Returns the number of characters written to the output buffer. */
    std::size_t size() const { return internal::to_unsigned(buffer_ - str_ + BUFFER_SIZE - 1); }

    /**
      Returns a pointer to the output buffer content. No terminating null
      character is appended.
     */
    const char *data() const { return str_; }

    /**
      Returns a pointer to the output buffer content with terminating null
      character appended.
     */
    const char *c_str() const {
        buffer_[BUFFER_SIZE - 1] = '\0';
        return str_;
    }

    /**
      \rst
      Returns the content of the output buffer as an ``std::string``.
      \endrst
     */
    std::string str() const { return std::string(str_, size()); }
};

// DEPRECATED!
// Formats a decimal integer value writing into buffer and returns
// a pointer to the end of the formatted string. This function doesn't
// write a terminating null character.
template <typename T>
inline void format_decimal(char *&buffer, T value) {
    typedef typename internal::int_traits<T>::main_type main_type;
    main_type                                           abs_value = static_cast<main_type>(value);
    if (internal::is_negative(value)) {
        *buffer++ = '-';
        abs_value = 0 - abs_value;
    }
    if (abs_value < 100) {
        if (abs_value < 10) {
            *buffer++ = static_cast<char>('0' + abs_value);
            return;
        }
        unsigned index = static_cast<unsigned>(abs_value * 2);
        *buffer++      = internal::data::DIGITS[index];
        *buffer++      = internal::data::DIGITS[index + 1];
        return;
    }
    int num_digits = internal::count_digits(abs_value);
    internal::format_decimal<char>(internal::make_checked(buffer, internal::to_unsigned(num_digits)), abs_value, num_digits);
    buffer += num_digits;
}

// Formatter of objects of type T.
template <typename T, typename Char>
struct formatter<T, Char, typename std::enable_if<internal::format_type<typename buffer_context<Char>::type, T>::value>::type> {
    // Parses format specifiers stopping either at the end of the range or at the
    // terminating '}'.
    template <typename ParseContext>
    constexpr typename ParseContext::iterator parse(ParseContext &ctx) {
        typedef internal::dynamic_specs_handler<ParseContext> handler_type;
        auto                                                  type = internal::get_type<typename buffer_context<Char>::type, T>::value;
        internal::specs_checker<handler_type>                 handler(handler_type(specs_, ctx), type);
        auto                                                  it        = parse_format_specs(ctx.begin(), ctx.end(), handler);
        auto                                                  type_spec = specs_.type;
        auto                                                  eh        = ctx.error_handler();
        switch (type) {
        case internal::none_type:
        case internal::named_arg_type: FMT_ASSERT(false, "invalid argument type"); break;
        case internal::int_type:
        case internal::uint_type:
        case internal::long_long_type:
        case internal::ulong_long_type:
        case internal::bool_type: handle_int_type_spec(type_spec, internal::int_type_checker<decltype(eh)>(eh)); break;
        case internal::char_type: handle_char_specs(&specs_, internal::char_specs_checker<decltype(eh)>(type_spec, eh)); break;
        case internal::double_type:
        case internal::long_double_type: handle_float_type_spec(type_spec, internal::float_type_checker<decltype(eh)>(eh)); break;
        case internal::cstring_type: internal::handle_cstring_type_spec(type_spec, internal::cstring_type_checker<decltype(eh)>(eh)); break;
        case internal::string_type: internal::check_string_type_spec(type_spec, eh); break;
        case internal::pointer_type: internal::check_pointer_type_spec(type_spec, eh); break;
        case internal::custom_type:
            // Custom format specifiers should be checked in parse functions of
            // formatter specializations.
            break;
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const T &val, FormatContext &ctx) -> decltype(ctx.out()) {
        internal::handle_dynamic_spec<internal::width_checker>(specs_.width_, specs_.width_ref, ctx);
        internal::handle_dynamic_spec<internal::precision_checker>(specs_.precision, specs_.precision_ref, ctx);
        typedef output_range<typename FormatContext::iterator, typename FormatContext::char_type> range_type;
        return visit_format_arg(arg_formatter<range_type>(ctx, &specs_), internal::make_arg<FormatContext>(val));
    }

private:
    internal::dynamic_format_specs<Char> specs_;
};

// A formatter for types known only at run time such as variant alternatives.
//
// Usage:
//   typedef std::variant<int, std::string> variant;
//   template <>
//   struct formatter<variant>: dynamic_formatter<> {
//     void format(buffer &buf, const variant &v, context &ctx) {
//       visit([&](const auto &val) { format(buf, val, ctx); }, v);
//     }
//   };
template <typename Char = char>
class dynamic_formatter {
private:
    struct null_handler : internal::error_handler {
        void on_align(alignment) {}
        void on_plus() {}
        void on_minus() {}
        void on_space() {}
        void on_hash() {}
    };

public:
    template <typename ParseContext>
    auto parse(ParseContext &ctx) -> decltype(ctx.begin()) {
        // Checks are deferred to formatting time when the argument type is known.
        internal::dynamic_specs_handler<ParseContext> handler(specs_, ctx);
        return parse_format_specs(ctx.begin(), ctx.end(), handler);
    }

    template <typename T, typename FormatContext>
    auto format(const T &val, FormatContext &ctx) -> decltype(ctx.out()) {
        handle_specs(ctx);
        internal::specs_checker<null_handler> checker(null_handler(), internal::get_type<FormatContext, T>::value);
        checker.on_align(specs_.align());
        if (specs_.flags == 0)
            ;  // Do nothing.
        else if (specs_.has(SIGN_FLAG))
            specs_.has(PLUS_FLAG) ? checker.on_plus() : checker.on_space();
        else if (specs_.has(MINUS_FLAG))
            checker.on_minus();
        else if (specs_.has(HASH_FLAG))
            checker.on_hash();
        if (specs_.precision != -1) checker.end_precision();
        typedef output_range<typename FormatContext::iterator, typename FormatContext::char_type> range;
        visit_format_arg(arg_formatter<range>(ctx, &specs_), internal::make_arg<FormatContext>(val));
        return ctx.out();
    }

private:
    template <typename Context>
    void handle_specs(Context &ctx) {
        internal::handle_dynamic_spec<internal::width_checker>(specs_.width_, specs_.width_ref, ctx);
        internal::handle_dynamic_spec<internal::precision_checker>(specs_.precision, specs_.precision_ref, ctx);
    }

    internal::dynamic_format_specs<Char> specs_;
};

template <typename Range, typename Char>
typename basic_format_context<Range, Char>::format_arg basic_format_context<Range, Char>::get_arg(basic_string_view<char_type> name) {
    map_.init(this->args());
    format_arg arg = map_.find(name);
    if (arg.type() == internal::none_type) this->on_error("argument not found");
    return arg;
}

template <typename ArgFormatter, typename Char, typename Context>
struct format_handler : internal::error_handler {
    typedef typename ArgFormatter::range range;

    format_handler(range r, basic_string_view<Char> str, basic_format_args<Context> format_args, internal::locale_ref loc)
        : context(r.begin(), str, format_args, loc) {}

    void on_text(const Char *begin, const Char *end) {
        auto   size = internal::to_unsigned(end - begin);
        auto   out  = context.out();
        auto &&it   = internal::reserve(out, size);
        it          = std::copy_n(begin, size, it);
        context.advance_to(out);
    }

    void on_arg_id() { arg = context.next_arg(); }
    void on_arg_id(unsigned id) {
        context.parse_context().check_arg_id(id);
        arg = context.get_arg(id);
    }
    void on_arg_id(basic_string_view<Char> id) { arg = context.get_arg(id); }

    void on_replacement_field(const Char *p) {
        context.parse_context().advance_to(p);
        internal::custom_formatter<Char, Context> f(context);
        if (!visit_format_arg(f, arg)) context.advance_to(visit_format_arg(ArgFormatter(context), arg));
    }

    const Char *on_format_specs(const Char *begin, const Char *end) {
        auto &parse_ctx = context.parse_context();
        parse_ctx.advance_to(begin);
        internal::custom_formatter<Char, Context> f(context);
        if (visit_format_arg(f, arg)) return parse_ctx.begin();
        basic_format_specs<Char> specs;
        using internal::specs_handler;
        internal::specs_checker<specs_handler<Context>> handler(specs_handler<Context>(specs, context), arg.type());
        begin = parse_format_specs(begin, end, handler);
        if (begin == end || *begin != '}') on_error("missing '}' in format string");
        parse_ctx.advance_to(begin);
        context.advance_to(visit_format_arg(ArgFormatter(context, &specs), arg));
        return begin;
    }

    Context                   context;
    basic_format_arg<Context> arg;
};

/** Formats arguments and writes the output to the range. */
template <typename ArgFormatter, typename Char, typename Context>
typename Context::iterator vformat_to(typename ArgFormatter::range out, basic_string_view<Char> format_str, basic_format_args<Context> args,
                                      internal::locale_ref loc = internal::locale_ref()) {
    format_handler<ArgFormatter, Char, Context> h(out, format_str, args, loc);
    internal::parse_format_string<false>(format_str, h);
    return h.context.out();
}

// Casts ``p`` to ``const void*`` for pointer formatting.
// Example:
//   auto s = format("{}", ptr(p));
template <typename T>
inline const void *ptr(const T *p) {
    return p;
}

template <typename It, typename Char>
struct arg_join {
    It                      begin;
    It                      end;
    basic_string_view<Char> sep;

    arg_join(It begin, It end, basic_string_view<Char> sep) : begin(begin), end(end), sep(sep) {}
};

template <typename It, typename Char>
struct formatter<arg_join<It, Char>, Char> : formatter<typename std::iterator_traits<It>::value_type, Char> {
    template <typename FormatContext>
    auto format(const arg_join<It, Char> &value, FormatContext &ctx) -> decltype(ctx.out()) {
        typedef formatter<typename std::iterator_traits<It>::value_type, Char> base;
        auto                                                                   it  = value.begin;
        auto                                                                   out = ctx.out();
        if (it != value.end) {
            out = base::format(*it++, ctx);
            while (it != value.end) {
                out = std::copy(value.sep.begin(), value.sep.end(), out);
                ctx.advance_to(out);
                out = base::format(*it++, ctx);
            }
        }
        return out;
    }
};

template <typename It>
arg_join<It, char> join(It begin, It end, string_view sep) {
    return arg_join<It, char>(begin, end, sep);
}

template <typename It>
arg_join<It, wchar_t> join(It begin, It end, wstring_view sep) {
    return arg_join<It, wchar_t>(begin, end, sep);
}

// The following causes ICE in gcc 4.4.
#if FMT_USE_TRAILING_RETURN && (!FMT_GCC_VERSION || FMT_GCC_VERSION >= 405)
template <typename Range>
auto join(const Range &range, string_view sep) -> arg_join<decltype(internal::begin(range)), char> {
    return join(internal::begin(range), internal::end(range), sep);
}

template <typename Range>
auto join(const Range &range, wstring_view sep) -> arg_join<decltype(internal::begin(range)), wchar_t> {
    return join(internal::begin(range), internal::end(range), sep);
}
#endif

/**
  \rst
  Converts *value* to ``std::string`` using the default format for type *T*.
  It doesn't support user-defined types with custom formatters.

  **Example**::

    #include <fmt/format.h>

    std::string answer = fmt::to_string(42);
  \endrst
 */
template <typename T>
std::string to_string(const T &value) {
    std::string                             str;
    internal::container_buffer<std::string> buf(str);
    writer(buf).write(value);
    return str;
}

/**
  Converts *value* to ``std::wstring`` using the default format for type *T*.
 */
template <typename T>
std::wstring to_wstring(const T &value) {
    std::wstring                             str;
    internal::container_buffer<std::wstring> buf(str);
    wwriter(buf).write(value);
    return str;
}

template <typename Char, std::size_t SIZE>
std::basic_string<Char> to_string(const basic_memory_buffer<Char, SIZE> &buf) {
    return std::basic_string<Char>(buf.data(), buf.size());
}

template <typename Char>
typename buffer_context<Char>::type::iterator internal::vformat_to(internal::basic_buffer<Char> &buf, basic_string_view<Char> format_str,
                                                                   basic_format_args<typename buffer_context<Char>::type> args) {
    typedef back_insert_range<internal::basic_buffer<Char>> range;
    return vformat_to<arg_formatter<range>>(buf, to_string_view(format_str), args);
}

template <typename S, typename Char = FMT_CHAR(S)>
inline typename buffer_context<Char>::type::iterator vformat_to(internal::basic_buffer<Char> &buf, const S &format_str,
                                                                basic_format_args<typename buffer_context<Char>::type> args) {
    return internal::vformat_to(buf, to_string_view(format_str), args);
}

template <typename S, typename... Args, std::size_t SIZE = inline_buffer_size, typename Char = typename internal::char_t<S>::type>
inline typename buffer_context<Char>::type::iterator format_to(basic_memory_buffer<Char, SIZE> &buf, const S &format_str, const Args &... args) {
    internal::check_format_string<Args...>(format_str);
    typedef typename buffer_context<Char>::type context;
    format_arg_store<context, Args...>          as{args...};
    return internal::vformat_to(buf, to_string_view(format_str), basic_format_args<context>(as));
}

namespace internal {

// Detect the iterator category of *any* given type in a SFINAE-friendly way.
// Unfortunately, older implementations of std::iterator_traits are not safe
// for use in a SFINAE-context.

// the gist of C++17's void_t magic
template <typename... Ts>
struct void_ {
    typedef void type;
};

template <typename T, typename Enable = void>
struct it_category : std::false_type {};

template <typename T>
struct it_category<T *> {
    typedef std::random_access_iterator_tag type;
};

template <typename T>
struct it_category<T, typename void_<typename T::iterator_category>::type> {
    typedef typename T::iterator_category type;
};

// Detect if *any* given type models the OutputIterator concept.
template <typename It>
class is_output_iterator {
    // Check for mutability because all iterator categories derived from
    // std::input_iterator_tag *may* also meet the requirements of an
    // OutputIterator, thereby falling into the category of 'mutable iterators'
    // [iterator.requirements.general] clause 4.
    // The compiler reveals this property only at the point of *actually
    // dereferencing* the iterator!
    template <typename U>
    static decltype(*(std::declval<U>())) test(std::input_iterator_tag);
    template <typename U>
    static char &test(std::output_iterator_tag);
    template <typename U>
    static const char &test(...);

    typedef decltype(test<It>(typename it_category<It>::type{})) type;
    typedef typename std::remove_reference<type>::type           result;

public:
    static const bool value = !std::is_const<result>::value;
};
}  // namespace internal

template <typename OutputIt, typename Char = char>
// using format_context_t = basic_format_context<OutputIt, Char>;
struct format_context_t {
    typedef basic_format_context<OutputIt, Char> type;
};

template <typename OutputIt, typename Char = char>
// using format_args_t = basic_format_args<format_context_t<OutputIt, Char>>;
struct format_args_t {
    typedef basic_format_args<typename format_context_t<OutputIt, Char>::type> type;
};

template <typename String, typename OutputIt, typename... Args>
inline typename std::enable_if<internal::is_output_iterator<OutputIt>::value, OutputIt>::type vformat_to(
    OutputIt out, const String &format_str, typename format_args_t<OutputIt, FMT_CHAR(String)>::type args) {
    typedef output_range<OutputIt, FMT_CHAR(String)> range;
    return vformat_to<arg_formatter<range>>(range(out), to_string_view(format_str), args);
}

/**
 \rst
 Formats arguments, writes the result to the output iterator ``out`` and returns
 the iterator past the end of the output range.

 **Example**::

   std::vector<char> out;
   fmt::format_to(std::back_inserter(out), "{}", 42);
 \endrst
 */
template <typename OutputIt, typename S, typename... Args>
inline FMT_ENABLE_IF_T(internal::is_string<S>::value &&internal::is_output_iterator<OutputIt>::value, OutputIt)
    format_to(OutputIt out, const S &format_str, const Args &... args) {
    internal::check_format_string<Args...>(format_str);
    typedef typename format_context_t<OutputIt, FMT_CHAR(S)>::type context;
    format_arg_store<context, Args...>                             as{args...};
    return vformat_to(out, to_string_view(format_str), basic_format_args<context>(as));
}

template <typename OutputIt>
struct format_to_n_result {
    /** Iterator past the end of the output range. */
    OutputIt out;
    /** Total (not truncated) output size. */
    std::size_t size;
};

template <typename OutputIt, typename Char = typename OutputIt::value_type>
struct format_to_n_context : format_context_t<fmt::internal::truncating_iterator<OutputIt>, Char> {};

template <typename OutputIt, typename Char = typename OutputIt::value_type>
struct format_to_n_args {
    typedef basic_format_args<typename format_to_n_context<OutputIt, Char>::type> type;
};

template <typename OutputIt, typename Char, typename... Args>
inline format_arg_store<typename format_to_n_context<OutputIt, Char>::type, Args...> make_format_to_n_args(const Args &... args) {
    return format_arg_store<typename format_to_n_context<OutputIt, Char>::type, Args...>(args...);
}

template <typename OutputIt, typename Char, typename... Args>
inline typename std::enable_if<internal::is_output_iterator<OutputIt>::value, format_to_n_result<OutputIt>>::type vformat_to_n(
    OutputIt out, std::size_t n, basic_string_view<Char> format_str, typename format_to_n_args<OutputIt, Char>::type args) {
    typedef internal::truncating_iterator<OutputIt> It;
    auto                                            it = vformat_to(It(out, n), format_str, args);
    return {it.base(), it.count()};
}

/**
 \rst
 Formats arguments, writes up to ``n`` characters of the result to the output
 iterator ``out`` and returns the total output size and the iterator past the
 end of the output range.
 \endrst
 */
template <typename OutputIt, typename S, typename... Args>
inline FMT_ENABLE_IF_T(internal::is_string<S>::value &&internal::is_output_iterator<OutputIt>::value, format_to_n_result<OutputIt>)
    format_to_n(OutputIt out, std::size_t n, const S &format_str, const Args &... args) {
    internal::check_format_string<Args...>(format_str);
    typedef FMT_CHAR(S) Char;
    format_arg_store<typename format_to_n_context<OutputIt, Char>::type, Args...> as(args...);
    return vformat_to_n(out, n, to_string_view(format_str), typename format_to_n_args<OutputIt, Char>::type(as));
}

template <typename Char>
inline std::basic_string<Char> internal::vformat(basic_string_view<Char> format_str, basic_format_args<typename buffer_context<Char>::type> args) {
    basic_memory_buffer<Char> buffer;
    internal::vformat_to(buffer, format_str, args);
    return fmt::to_string(buffer);
}

/**
  Returns the number of characters in the output of
  ``format(format_str, args...)``.
 */
template <typename... Args>
inline std::size_t formatted_size(string_view format_str, const Args &... args) {
    auto it = format_to(internal::counting_iterator<char>(), format_str, args...);
    return it.count();
}

FMT_END_NAMESPACE

#define FMT_STRING(s)                                                                                                \
    [] {                                                                                                             \
        typedef typename std::remove_cv<std::remove_pointer<typename std::decay<decltype(s)>::type>::type>::type ct; \
        struct str : fmt::compile_string {                                                                           \
            typedef ct char_type;                                                                                    \
            constexpr  operator fmt::basic_string_view<ct>() const { return {s, sizeof(s) / sizeof(ct) - 1}; }       \
        };                                                                                                           \
        return str{};                                                                                                \
    }()

#if defined(FMT_STRING_ALIAS) && FMT_STRING_ALIAS
/**
  \rst
  Constructs a compile-time format string. This macro is disabled by default to
  prevent potential name collisions. To enable it define ``FMT_STRING_ALIAS`` to
  1 before including ``fmt/format.h``.

  **Example**::

    #define FMT_STRING_ALIAS 1
    #include <fmt/format.h>
    // A compile-time error because 'd' is an invalid specifier for strings.
    std::string s = format(fmt("{:d}"), "foo");
  \endrst
 */
#define fmt(s) FMT_STRING(s)
#endif

#define FMT_FUNC inline

// Formatting library for C++
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_FORMAT_INL_H_
#define FMT_FORMAT_INL_H_

#include <string.h>

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>  // for std::ptrdiff_t
#include <cstring>  // for std::memmove
#if !defined(FMT_STATIC_THOUSANDS_SEPARATOR)
#include <locale>
#endif

#if FMT_USE_WINDOWS_H
#if !defined(FMT_HEADER_ONLY) && !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if defined(NOMINMAX) || defined(FMT_WIN_MINMAX)
#include <windows.h>
#else
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#endif
#endif

#define FMT_TRY if (true)
#define FMT_CATCH(x) if (false)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
#pragma warning(disable : 4702)  // unreachable code
// Disable deprecation warning for strerror. The latter is not called but
// MSVC fails to detect it.
#pragma warning(disable : 4996)
#endif

FMT_BEGIN_NAMESPACE

namespace {
#ifndef _MSC_VER
#define FMT_SNPRINTF snprintf
#else  // _MSC_VER
inline int fmt_snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf_s(buffer, size, _TRUNCATE, format, args);
    va_end(args);
    return result;
}
#define FMT_SNPRINTF fmt_snprintf
#endif  // _MSC_VER
}  // namespace

FMT_FUNC size_t internal::count_code_points(basic_string_view<char8_t> s) {
    const char8_t *data            = s.data();
    size_t         num_code_points = 0;
    for (size_t i = 0, size = s.size(); i != size; ++i) {
        if ((data[i] & 0xc0) != 0x80) ++num_code_points;
    }
    return num_code_points;
}

#if !defined(FMT_STATIC_THOUSANDS_SEPARATOR)
namespace internal {

template <typename Locale>
locale_ref::locale_ref(const Locale &loc) : locale_(&loc) {
    static_assert(std::is_same<Locale, std::locale>::value, "");
}

template <typename Locale>
Locale locale_ref::get() const {
    static_assert(std::is_same<Locale, std::locale>::value, "");
    return locale_ ? *static_cast<const std::locale *>(locale_) : std::locale();
}

template <typename Char>
FMT_FUNC Char thousands_sep_impl(locale_ref loc) {
    return std::use_facet<std::numpunct<Char>>(loc.get<std::locale>()).thousands_sep();
}
}  // namespace internal
#else
template <typename Char>
FMT_FUNC Char internal::thousands_sep_impl(locale_ref) {
    return FMT_STATIC_THOUSANDS_SEPARATOR;
}
#endif

namespace internal {
template <typename T>
int char_traits<char>::format_float(char *buf, std::size_t size, const char *format, int precision, T value) {
    return precision < 0 ? FMT_SNPRINTF(buf, size, format, value) : FMT_SNPRINTF(buf, size, format, precision, value);
}

template <typename T>
int char_traits<wchar_t>::format_float(wchar_t *buf, std::size_t size, const wchar_t *format, int precision, T value) {
    return precision < 0 ? FMT_SWPRINTF(buf, size, format, value) : FMT_SWPRINTF(buf, size, format, precision, value);
}

template <typename T>
const char basic_data<T>::DIGITS[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

#define FMT_POWERS_OF_10(factor) \
    factor * 10, factor * 100, factor * 1000, factor * 10000, factor * 100000, factor * 1000000, factor * 10000000, factor * 100000000, factor * 1000000000

template <typename T>
const uint32_t basic_data<T>::POWERS_OF_10_32[] = {1, FMT_POWERS_OF_10(1)};

template <typename T>
const uint32_t basic_data<T>::ZERO_OR_POWERS_OF_10_32[] = {0, FMT_POWERS_OF_10(1)};

template <typename T>
const uint64_t basic_data<T>::ZERO_OR_POWERS_OF_10_64[] = {0, FMT_POWERS_OF_10(1), FMT_POWERS_OF_10(1000000000ull), 10000000000000000000ull};

// Normalized 64-bit significands of pow(10, k), for k = -348, -340, ..., 340.
// These are generated by support/compute-powers.py.
template <typename T>
const uint64_t basic_data<T>::POW10_SIGNIFICANDS[] = {
    0xfa8fd5a0081c0288, 0xbaaee17fa23ebf76, 0x8b16fb203055ac76, 0xcf42894a5dce35ea, 0x9a6bb0aa55653b2d, 0xe61acf033d1a45df, 0xab70fe17c79ac6ca,
    0xff77b1fcbebcdc4f, 0xbe5691ef416bd60c, 0x8dd01fad907ffc3c, 0xd3515c2831559a83, 0x9d71ac8fada6c9b5, 0xea9c227723ee8bcb, 0xaecc49914078536d,
    0x823c12795db6ce57, 0xc21094364dfb5637, 0x9096ea6f3848984f, 0xd77485cb25823ac7, 0xa086cfcd97bf97f4, 0xef340a98172aace5, 0xb23867fb2a35b28e,
    0x84c8d4dfd2c63f3b, 0xc5dd44271ad3cdba, 0x936b9fcebb25c996, 0xdbac6c247d62a584, 0xa3ab66580d5fdaf6, 0xf3e2f893dec3f126, 0xb5b5ada8aaff80b8,
    0x87625f056c7c4a8b, 0xc9bcff6034c13053, 0x964e858c91ba2655, 0xdff9772470297ebd, 0xa6dfbd9fb8e5b88f, 0xf8a95fcf88747d94, 0xb94470938fa89bcf,
    0x8a08f0f8bf0f156b, 0xcdb02555653131b6, 0x993fe2c6d07b7fac, 0xe45c10c42a2b3b06, 0xaa242499697392d3, 0xfd87b5f28300ca0e, 0xbce5086492111aeb,
    0x8cbccc096f5088cc, 0xd1b71758e219652c, 0x9c40000000000000, 0xe8d4a51000000000, 0xad78ebc5ac620000, 0x813f3978f8940984, 0xc097ce7bc90715b3,
    0x8f7e32ce7bea5c70, 0xd5d238a4abe98068, 0x9f4f2726179a2245, 0xed63a231d4c4fb27, 0xb0de65388cc8ada8, 0x83c7088e1aab65db, 0xc45d1df942711d9a,
    0x924d692ca61be758, 0xda01ee641a708dea, 0xa26da3999aef774a, 0xf209787bb47d6b85, 0xb454e4a179dd1877, 0x865b86925b9bc5c2, 0xc83553c5c8965d3d,
    0x952ab45cfa97a0b3, 0xde469fbd99a05fe3, 0xa59bc234db398c25, 0xf6c69a72a3989f5c, 0xb7dcbf5354e9bece, 0x88fcf317f22241e2, 0xcc20ce9bd35c78a5,
    0x98165af37b2153df, 0xe2a0b5dc971f303a, 0xa8d9d1535ce3b396, 0xfb9b7cd9a4a7443c, 0xbb764c4ca7a44410, 0x8bab8eefb6409c1a, 0xd01fef10a657842c,
    0x9b10a4e5e9913129, 0xe7109bfba19c0c9d, 0xac2820d9623bf429, 0x80444b5e7aa7cf85, 0xbf21e44003acdd2d, 0x8e679c2f5e44ff8f, 0xd433179d9c8cb841,
    0x9e19db92b4e31ba9, 0xeb96bf6ebadf77d9, 0xaf87023b9bf0ee6b,
};

// Binary exponents of pow(10, k), for k = -348, -340, ..., 340, corresponding
// to significands above.
template <typename T>
const int16_t basic_data<T>::POW10_EXPONENTS[] = {
    -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007, -980, -954, -927, -901, -874, -847, -821, -794, -768, -741, -715, -688, -661,
    -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,  -422,  -396, -369, -343, -316, -289, -263, -236, -210, -183, -157, -130, -103, -77,
    -50,   -24,   3,     30,    56,    83,    109,   136,   162,   189,  216,  242,  269,  295,  322,  348,  375,  402,  428,  455,  481,  508,
    534,   561,   588,   614,   641,   667,   694,   720,   747,   774,  800,  827,  853,  880,  907,  933,  960,  986,  1013, 1039, 1066};

template <typename T>
const char basic_data<T>::FOREGROUND_COLOR[] = "\x1b[38;2;";
template <typename T>
const char basic_data<T>::BACKGROUND_COLOR[] = "\x1b[48;2;";
template <typename T>
const char basic_data<T>::RESET_COLOR[] = "\x1b[0m";
template <typename T>
const wchar_t basic_data<T>::WRESET_COLOR[] = L"\x1b[0m";

// A handmade floating-point number f * pow(2, e).
class fp {
private:
    typedef uint64_t significand_type;

    // All sizes are in bits.
    static constexpr const int char_size = std::numeric_limits<unsigned char>::digits;
    // Subtract 1 to account for an implicit most significant bit in the
    // normalized form.
    static constexpr const int      double_significand_size = std::numeric_limits<double>::digits - 1;
    static constexpr const uint64_t implicit_bit            = 1ull << double_significand_size;

public:
    significand_type f;
    int              e;

    static constexpr const int significand_size = sizeof(significand_type) * char_size;

    fp() : f(0), e(0) {}
    fp(uint64_t f_val, int e_val) : f(f_val), e(e_val) {}

    // Constructs fp from an IEEE754 double. It is a template to prevent compile
    // errors on platforms where double is not IEEE754.
    template <typename Double>
    explicit fp(Double d) {
        // Assume double is in the format [sign][exponent][significand].
        typedef std::numeric_limits<Double> limits;
        const int                           double_size      = static_cast<int>(sizeof(Double) * char_size);
        const int                           exponent_size    = double_size - double_significand_size - 1;  // -1 for sign
        const uint64_t                      significand_mask = implicit_bit - 1;
        const uint64_t                      exponent_mask    = (~0ull >> 1) & ~significand_mask;
        const int                           exponent_bias    = (1 << exponent_size) - limits::max_exponent - 1;
        auto                                u                = bit_cast<uint64_t>(d);
        auto                                biased_e         = (u & exponent_mask) >> double_significand_size;
        f                                                    = u & significand_mask;
        if (biased_e != 0)
            f += implicit_bit;
        else
            biased_e = 1;  // Subnormals use biased exponent 1 (min exponent).
        e = static_cast<int>(biased_e - exponent_bias - double_significand_size);
    }

    // Normalizes the value converted from double and multiplied by (1 << SHIFT).
    template <int SHIFT = 0>
    void normalize() {
        // Handle subnormals.
        auto shifted_implicit_bit = implicit_bit << SHIFT;
        while ((f & shifted_implicit_bit) == 0) {
            f <<= 1;
            --e;
        }
        // Subtract 1 to account for hidden bit.
        auto offset = significand_size - double_significand_size - SHIFT - 1;
        f <<= offset;
        e -= offset;
    }

    // Compute lower and upper boundaries (m^- and m^+ in the Grisu paper), where
    // a boundary is a value half way between the number and its predecessor
    // (lower) or successor (upper). The upper boundary is normalized and lower
    // has the same exponent but may be not normalized.
    void compute_boundaries(fp &lower, fp &upper) const {
        lower = f == implicit_bit ? fp((f << 2) - 1, e - 2) : fp((f << 1) - 1, e - 1);
        upper = fp((f << 1) + 1, e - 1);
        upper.normalize<1>();  // 1 is to account for the exponent shift above.
        lower.f <<= lower.e - upper.e;
        lower.e = upper.e;
    }
};

// Returns an fp number representing x - y. Result may not be normalized.
inline fp operator-(fp x, fp y) {
    FMT_ASSERT(x.f >= y.f && x.e == y.e, "invalid operands");
    return fp(x.f - y.f, x.e);
}

// Computes an fp number r with r.f = x.f * y.f / pow(2, 64) rounded to nearest
// with half-up tie breaking, r.e = x.e + y.e + 64. Result may not be normalized.
FMT_API fp operator*(fp x, fp y);

// Returns cached power (of 10) c_k = c_k.f * pow(2, c_k.e) such that its
// (binary) exponent satisfies min_exponent <= c_k.e <= min_exponent + 3.
FMT_API fp get_cached_power(int min_exponent, int &pow10_exponent);

FMT_FUNC fp operator*(fp x, fp y) {
    // Multiply 32-bit parts of significands.
    uint64_t mask = (1ULL << 32) - 1;
    uint64_t a = x.f >> 32, b = x.f & mask;
    uint64_t c = y.f >> 32, d = y.f & mask;
    uint64_t ac = a * c, bc = b * c, ad = a * d, bd = b * d;
    // Compute mid 64-bit of result and round.
    uint64_t mid = (bd >> 32) + (ad & mask) + (bc & mask) + (1U << 31);
    return fp(ac + (ad >> 32) + (bc >> 32) + (mid >> 32), x.e + y.e + 64);
}

FMT_FUNC fp get_cached_power(int min_exponent, int &pow10_exponent) {
    const double one_over_log2_10 = 0.30102999566398114;  // 1 / log2(10)
    int          index            = static_cast<int>(std::ceil((min_exponent + fp::significand_size - 1) * one_over_log2_10));
    // Decimal exponent of the first (smallest) cached power of 10.
    const int first_dec_exp = -348;
    // Difference between 2 consecutive decimal exponents in cached powers of 10.
    const int dec_exp_step = 8;
    index                  = (index - first_dec_exp - 1) / dec_exp_step + 1;
    pow10_exponent         = first_dec_exp + index * dec_exp_step;
    return fp(data::POW10_SIGNIFICANDS[index], data::POW10_EXPONENTS[index]);
}

FMT_FUNC bool grisu2_round(char *buf, int &size, int max_digits, uint64_t delta, uint64_t remainder, uint64_t exp, uint64_t diff, int &exp10) {
    while (remainder < diff && delta - remainder >= exp && (remainder + exp < diff || diff - remainder > remainder + exp - diff)) {
        --buf[size - 1];
        remainder += exp;
    }
    if (size > max_digits) {
        --size;
        ++exp10;
        if (buf[size] >= '5') return false;
    }
    return true;
}

// Generates output using Grisu2 digit-gen algorithm.
FMT_FUNC bool grisu2_gen_digits(char *buf, int &size, uint32_t hi, uint64_t lo, int &exp, uint64_t delta, const fp &one, const fp &diff, int max_digits) {
    // Generate digits for the most significant part (hi).
    while (exp > 0) {
        uint32_t digit = 0;
        // This optimization by miloyip reduces the number of integer divisions by
        // one per iteration.
        switch (exp) {
        case 10:
            digit = hi / 1000000000;
            hi %= 1000000000;
            break;
        case 9:
            digit = hi / 100000000;
            hi %= 100000000;
            break;
        case 8:
            digit = hi / 10000000;
            hi %= 10000000;
            break;
        case 7:
            digit = hi / 1000000;
            hi %= 1000000;
            break;
        case 6:
            digit = hi / 100000;
            hi %= 100000;
            break;
        case 5:
            digit = hi / 10000;
            hi %= 10000;
            break;
        case 4:
            digit = hi / 1000;
            hi %= 1000;
            break;
        case 3:
            digit = hi / 100;
            hi %= 100;
            break;
        case 2:
            digit = hi / 10;
            hi %= 10;
            break;
        case 1:
            digit = hi;
            hi    = 0;
            break;
        default: FMT_ASSERT(false, "invalid number of digits");
        }
        if (digit != 0 || size != 0) buf[size++] = static_cast<char>('0' + digit);
        --exp;
        uint64_t remainder = (static_cast<uint64_t>(hi) << -one.e) + lo;
        if (remainder <= delta || size > max_digits) {
            return grisu2_round(buf, size, max_digits, delta, remainder, static_cast<uint64_t>(data::POWERS_OF_10_32[exp]) << -one.e, diff.f, exp);
        }
    }
    // Generate digits for the least significant part (lo).
    for (;;) {
        lo *= 10;
        delta *= 10;
        char digit = static_cast<char>(lo >> -one.e);
        if (digit != 0 || size != 0) buf[size++] = static_cast<char>('0' + digit);
        lo &= one.f - 1;
        --exp;
        if (lo < delta || size > max_digits) { return grisu2_round(buf, size, max_digits, delta, lo, one.f, diff.f * data::POWERS_OF_10_32[-exp], exp); }
    }
}

struct gen_digits_params {
    int  num_digits;
    bool fixed;
    bool upper;
    bool trailing_zeros;
};

struct prettify_handler {
    char *    data;
    ptrdiff_t size;
    buffer &  buf;

    explicit prettify_handler(buffer &b, ptrdiff_t n) : data(b.data()), size(n), buf(b) {}
    ~prettify_handler() {
        assert(buf.size() >= to_unsigned(size));
        buf.resize(to_unsigned(size));
    }

    template <typename F>
    void insert(ptrdiff_t pos, ptrdiff_t n, F f) {
        std::memmove(data + pos + n, data + pos, to_unsigned(size - pos));
        f(data + pos);
        size += n;
    }

    void insert(ptrdiff_t pos, char c) {
        std::memmove(data + pos + 1, data + pos, to_unsigned(size - pos));
        data[pos] = c;
        ++size;
    }

    void append(ptrdiff_t n, char c) {
        std::uninitialized_fill_n(data + size, n, c);
        size += n;
    }

    void append(char c) { data[size++] = c; }

    void remove_trailing(char c) {
        while (data[size - 1] == c) --size;
    }
};

// Writes the exponent exp in the form "[+-]d{2,3}" to buffer.
template <typename Handler>
FMT_FUNC void write_exponent(int exp, Handler &&h) {
    FMT_ASSERT(-1000 < exp && exp < 1000, "exponent out of range");
    if (exp < 0) {
        h.append('-');
        exp = -exp;
    } else {
        h.append('+');
    }
    if (exp >= 100) {
        h.append(static_cast<char>('0' + exp / 100));
        exp %= 100;
        const char *d = data::DIGITS + exp * 2;
        h.append(d[0]);
        h.append(d[1]);
    } else {
        const char *d = data::DIGITS + exp * 2;
        h.append(d[0]);
        h.append(d[1]);
    }
}

struct fill {
    size_t n;
    void   operator()(char *buf) const {
        buf[0] = '0';
        buf[1] = '.';
        std::uninitialized_fill_n(buf + 2, n, '0');
    }
};

// The number is given as v = f * pow(10, exp), where f has size digits.
template <typename Handler>
FMT_FUNC void grisu2_prettify(const gen_digits_params &params, int size, int exp, Handler &&handler) {
    if (!params.fixed) {
        // Insert a decimal point after the first digit and add an exponent.
        handler.insert(1, '.');
        exp += size - 1;
        if (size < params.num_digits) handler.append(params.num_digits - size, '0');
        handler.append(params.upper ? 'E' : 'e');
        write_exponent(exp, handler);
        return;
    }
    // pow(10, full_exp - 1) <= v <= pow(10, full_exp).
    int       full_exp      = size + exp;
    const int exp_threshold = 21;
    if (size <= full_exp && full_exp <= exp_threshold) {
        // 1234e7 -> 12340000000[.0+]
        handler.append(full_exp - size, '0');
        int num_zeros = params.num_digits - full_exp;
        if (num_zeros > 0 && params.trailing_zeros) {
            handler.append('.');
            handler.append(num_zeros, '0');
        }
    } else if (full_exp > 0) {
        // 1234e-2 -> 12.34[0+]
        handler.insert(full_exp, '.');
        if (!params.trailing_zeros) {
            // Remove trailing zeros.
            handler.remove_trailing('0');
        } else if (params.num_digits > size) {
            // Add trailing zeros.
            ptrdiff_t num_zeros = params.num_digits - size;
            handler.append(num_zeros, '0');
        }
    } else {
        // 1234e-6 -> 0.001234
        handler.insert(0, 2 - full_exp, fill{to_unsigned(-full_exp)});
    }
}

struct char_counter {
    ptrdiff_t size;

    template <typename F>
    void insert(ptrdiff_t, ptrdiff_t n, F) {
        size += n;
    }
    void insert(ptrdiff_t, char) { ++size; }
    void append(ptrdiff_t n, char) { size += n; }
    void append(char) { ++size; }
    void remove_trailing(char) {}
};

// Converts format specifiers into parameters for digit generation and computes
// output buffer size for a number in the range [pow(10, exp - 1), pow(10, exp)
// or 0 if exp == 1.
FMT_FUNC gen_digits_params process_specs(const core_format_specs &specs, int exp, buffer &buf) {
    auto params     = gen_digits_params();
    int  num_digits = specs.precision >= 0 ? specs.precision : 6;
    switch (specs.type) {
    case 'G': params.upper = true; [[fallthrough]];
    case '\0':
    case 'g':
        params.trailing_zeros = (specs.flags & HASH_FLAG) != 0;
        if (-4 <= exp && exp < num_digits + 1) {
            params.fixed = true;
            if (!specs.type && params.trailing_zeros && exp >= 0) num_digits = exp + 1;
        }
        break;
    case 'F': params.upper = true; [[fallthrough]];
    case 'f': {
        params.fixed            = true;
        params.trailing_zeros   = true;
        int adjusted_min_digits = num_digits + exp;
        if (adjusted_min_digits > 0) num_digits = adjusted_min_digits;
        break;
    }
    case 'E': params.upper = true; [[fallthrough]];
    case 'e': ++num_digits; break;
    }
    params.num_digits = num_digits;
    char_counter counter{num_digits};
    grisu2_prettify(params, params.num_digits, exp - num_digits, counter);
    buf.resize(to_unsigned(counter.size));
    return params;
}

template <typename Double>
FMT_FUNC typename std::enable_if<sizeof(Double) == sizeof(uint64_t), bool>::type grisu2_format(Double value, buffer &buf, core_format_specs specs) {
    FMT_ASSERT(value >= 0, "value is negative");
    if (value <= 0) {
        gen_digits_params params = process_specs(specs, 1, buf);
        const size_t      size   = 1;
        buf[0]                   = '0';
        grisu2_prettify(params, size, 0, prettify_handler(buf, size));
        return true;
    }

    fp fp_value(value);
    fp lower, upper;  // w^- and w^+ in the Grisu paper.
    fp_value.compute_boundaries(lower, upper);

    // Find a cached power of 10 close to 1 / upper and use it to scale upper.
    const int min_exp    = -60;               // alpha in Grisu.
    int       cached_exp = 0;                 // K in Grisu.
    auto      cached_pow = get_cached_power(  // \tilde{c}_{-k} in Grisu.
        min_exp - (upper.e + fp::significand_size), cached_exp);
    cached_exp           = -cached_exp;
    upper                = upper * cached_pow;  // \tilde{M}^+ in Grisu.
    --upper.f;                                  // \tilde{M}^+ - 1 ulp -> M^+_{\downarrow}.
    fp one(1ull << -upper.e, upper.e);
    // hi (p1 in Grisu) contains the most significant digits of scaled_upper.
    // hi = floor(upper / one).
    uint32_t          hi     = static_cast<uint32_t>(upper.f >> -one.e);
    int               exp    = count_digits(hi);  // kappa in Grisu.
    gen_digits_params params = process_specs(specs, cached_exp + exp, buf);
    fp_value.normalize();
    fp scaled_value = fp_value * cached_pow;
    lower           = lower * cached_pow;  // \tilde{M}^- in Grisu.
    ++lower.f;                             // \tilde{M}^- + 1 ulp -> M^-_{\uparrow}.
    uint64_t delta = upper.f - lower.f;
    fp       diff  = upper - scaled_value;  // wp_w in Grisu.
    // lo (p2 in Grisu) contains the least significants digits of scaled_upper.
    // lo = supper % one.
    uint64_t lo   = upper.f & (one.f - 1);
    int      size = 0;
    if (!grisu2_gen_digits(buf.data(), size, hi, lo, exp, delta, one, diff, params.num_digits)) {
        buf.clear();
        return false;
    }
    grisu2_prettify(params, size, cached_exp + exp, prettify_handler(buf, size));
    return true;
}

template <typename Double>
void sprintf_format(Double value, internal::buffer &buf, core_format_specs spec) {
    // Buffer capacity must be non-zero, otherwise MSVC's vsnprintf_s will fail.
    FMT_ASSERT(buf.capacity() != 0, "empty buffer");

    // Build format string.
    enum { MAX_FORMAT_SIZE = 10 };  // longest format: %#-*.*Lg
    char  format[MAX_FORMAT_SIZE];
    char *format_ptr = format;
    *format_ptr++    = '%';
    if (spec.has(HASH_FLAG)) *format_ptr++ = '#';
    if (spec.precision >= 0) {
        *format_ptr++ = '.';
        *format_ptr++ = '*';
    }
    if (std::is_same<Double, long double>::value) *format_ptr++ = 'L';
    *format_ptr++ = spec.type;
    *format_ptr   = '\0';

    // Format using snprintf.
    char *start = nullptr;
    for (;;) {
        std::size_t buffer_size = buf.capacity();
        start                   = &buf[0];
        int result              = internal::char_traits<char>::format_float(start, buffer_size, format, spec.precision, value);
        if (result >= 0) {
            unsigned n = internal::to_unsigned(result);
            if (n < buf.capacity()) {
                buf.resize(n);
                break;  // The buffer is large enough - continue with formatting.
            }
            buf.reserve(n + 1);
        } else {
            // If result is negative we ask to increase the capacity by at least 1,
            // but as std::vector, the buffer grows exponentially.
            buf.reserve(buf.capacity() + 1);
        }
    }
}
}  // namespace internal

#if FMT_USE_WINDOWS_H

FMT_FUNC internal::utf8_to_utf16::utf8_to_utf16(string_view s) {
    static const char ERROR_MSG[] = "cannot convert string from UTF-8 to UTF-16";
    if (s.size() > INT_MAX) FMT_THROW(windows_error(ERROR_INVALID_PARAMETER, ERROR_MSG));
    int s_size = static_cast<int>(s.size());
    if (s_size == 0) {
        // MultiByteToWideChar does not support zero length, handle separately.
        buffer_.resize(1);
        buffer_[0] = 0;
        return;
    }

    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), s_size, nullptr, 0);
    if (length == 0) FMT_THROW(windows_error(GetLastError(), ERROR_MSG));
    buffer_.resize(length + 1);
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), s_size, &buffer_[0], length);
    if (length == 0) FMT_THROW(windows_error(GetLastError(), ERROR_MSG));
    buffer_[length] = 0;
}

FMT_FUNC internal::utf16_to_utf8::utf16_to_utf8(wstring_view s) {
    if (int error_code = convert(s)) { FMT_THROW(windows_error(error_code, "cannot convert string from UTF-16 to UTF-8")); }
}

FMT_FUNC int internal::utf16_to_utf8::convert(wstring_view s) {
    if (s.size() > INT_MAX) return ERROR_INVALID_PARAMETER;
    int s_size = static_cast<int>(s.size());
    if (s_size == 0) {
        // WideCharToMultiByte does not support zero length, handle separately.
        buffer_.resize(1);
        buffer_[0] = 0;
        return 0;
    }

    int length = WideCharToMultiByte(CP_UTF8, 0, s.data(), s_size, nullptr, 0, nullptr, nullptr);
    if (length == 0) return GetLastError();
    buffer_.resize(length + 1);
    length = WideCharToMultiByte(CP_UTF8, 0, s.data(), s_size, &buffer_[0], length, nullptr, nullptr);
    if (length == 0) return GetLastError();
    buffer_[length] = 0;
    return 0;
}

FMT_FUNC void windows_error::init(int err_code, string_view format_str, format_args args) {
    error_code_ = err_code;
    memory_buffer buffer;
    internal::format_windows_error(buffer, err_code, vformat(format_str, args));
    std::runtime_error &base = *this;
    base                     = std::runtime_error(to_string(buffer));
}

FMT_FUNC void internal::format_windows_error(internal::buffer &out, int error_code, string_view message) noexcept {
    FMT_TRY {
        wmemory_buffer buf;
        buf.resize(inline_buffer_size);
        for (;;) {
            wchar_t *system_message = &buf[0];
            int      result         = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code,
                                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), system_message, static_cast<uint32_t>(buf.size()), nullptr);
            if (result != 0) {
                utf16_to_utf8 utf8_message;
                if (utf8_message.convert(system_message) == ERROR_SUCCESS) {
                    writer w(out);
                    w.write(message);
                    w.write(": ");
                    w.write(utf8_message);
                    return;
                }
                break;
            }
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;  // Can't get error message, report error code instead.
            buf.resize(buf.size() * 2);
        }
    }
    FMT_CATCH(...) {}
    format_error_code(out, error_code, message);
}

#endif  // FMT_USE_WINDOWS_H

void internal::error_handler::on_error(const char *message) { FMT_THROW(message); }

FMT_FUNC void vprint(std::FILE *f, string_view format_str, format_args args) {
    memory_buffer buffer;
    internal::vformat_to(buffer, format_str, basic_format_args<buffer_context<char>::type>(args));
    std::fwrite(buffer.data(), 1, buffer.size(), f);
}

FMT_FUNC void vprint(std::FILE *f, wstring_view format_str, wformat_args args) {
    wmemory_buffer buffer;
    internal::vformat_to(buffer, format_str, args);
    std::fwrite(buffer.data(), sizeof(wchar_t), buffer.size(), f);
}

FMT_FUNC void vprint(string_view format_str, format_args args) { vprint(stdout, format_str, args); }

FMT_FUNC void vprint(wstring_view format_str, wformat_args args) { vprint(stdout, format_str, args); }

FMT_END_NAMESPACE

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // FMT_FORMAT_INL_H_

// Restore warnings.
#if FMT_GCC_VERSION >= 406 || FMT_CLANG_VERSION
#pragma GCC diagnostic pop
#endif

#endif  // FMT_FORMAT_H_
