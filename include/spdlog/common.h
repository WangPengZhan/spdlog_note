// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/tweakme.h>
#include <spdlog/details/null_mutex.h>

#include <atomic>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <exception>
#include <string>
#include <type_traits>
#include <functional>

#ifdef SPDLOG_COMPILED_LIB
#undef SPDLOG_HEADER_ONLY
#if defined(_WIN32) && defined(SPDLOG_SHARED_LIB)
#ifdef spdlog_EXPORTS
#define SPDLOG_API __declspec(dllexport)
#else
#define SPDLOG_API __declspec(dllimport)
#endif
#else // !defined(_WIN32) || !defined(SPDLOG_SHARED_LIB)
#define SPDLOG_API    // SPDLOG API为空字符？
#endif
#define SPDLOG_INLINE
#else // !defined(SPDLOG_COMPILED_LIB)
#define SPDLOG_API
#define SPDLOG_HEADER_ONLY
#define SPDLOG_INLINE inline
#endif // #ifdef SPDLOG_COMPILED_LIB

/*
*   _declspec是Microsoft VC中专用的关键字，它配合着一些属性可以对标准C/C++进行扩充。
*   __declspec关键字应该出现在声明的前面。
*
*   __declspec(dllexport)用于Windows中的动态库中，声明导出函数、类、对象等供外面调用，省略给出.def文件。
*   即将函数、类等声明为导出函数，供其它程序调用，作为动态库的对外接口函数、类等。

*   .def文件(模块定义文件)是包含一个或多个描述各种DLL属性的Module语句的文本文件。
*   .def文件或__declspec(dllexport)都是将公共符号导入到应用程序或从DLL导出函数。
*   如果不提供__declspec(dllexport)导出DLL函数，则DLL需要提供.def文件。

*   __declspec(dllimport)用于Windows中，从别的动态库中声明导入函数、类、对象等供本动态库或exe文件使用。
*   当你需要使用DLL中的函数时，往往不需要显示地导入函数，编译器可自动完成。
*   不使用__declspec(dllimport)也能正确编译代码，但使用__declspec(dllimport)使编译器可以生成更好的代码。
*   编译器之所以能够生成更好的代码，是因为它可以确定函数是否存在于DLL中，这使得编译器可以生成跳过间接寻址级别的代码，
*   而这些代码通常会出现在跨DLL边界的函数调用中。
*   声明一个导入函数，是说这个函数是从别的DLL导入。一般用于使用某个DLL的exe中。
*
*/

#include <spdlog/fmt/fmt.h>

// visual studio upto 2013 does not support noexcept nor constexpr
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define SPDLOG_NOEXCEPT _NOEXCEPT
#define SPDLOG_CONSTEXPR
#else
#define SPDLOG_NOEXCEPT noexcept
#define SPDLOG_CONSTEXPR constexpr
#endif

#if defined(__GNUC__) || defined(__clang__)
#define SPDLOG_DEPRECATED __attribute__((deprecated)) //用__attribute__((deprecated)) 管理过时代码，同时保留兼容的接口
#elif defined(_MSC_VER)
#define SPDLOG_DEPRECATED __declspec(deprecated)
#else
#define SPDLOG_DEPRECATED
#endif

/*
*   在开发一些库的时候，API的接口可能会过时，为了提醒开发者这个函数已经过时。
*   可以在函数声明时加上attribute((deprecated))属性，如此，
*   只要函数被使用，在编译时都会产生警告，警告信息中包含过时接口的名称及代码中的引用位置。
*
*   attribute可以设置函数属性（Function Attribute）、变量属性（Variable Attribute）和类型属性（Type Attribute）。
*   attribute语法格式为：attribute ((attribute))
*   注意： 使用attribute的时候，只能函数的声明处使用attribute，
*/

// disable thread local on msvc 2013
#ifndef SPDLOG_NO_TLS
#if (defined(_MSC_VER) && (_MSC_VER < 1900)) || defined(__cplusplus_winrt)
#define SPDLOG_NO_TLS 1
#endif
#endif

#ifndef SPDLOG_FUNCTION
#define SPDLOG_FUNCTION static_cast<const char *>(__FUNCTION__)
#endif

#ifdef SPDLOG_NO_EXCEPTIONS
#define SPDLOG_TRY
#define SPDLOG_THROW(ex)                                                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("spdlog fatal error: %s\n", ex.what());                                                                                     \
        std::abort();                                                                                                                      \
    } while (0)
#define SPDLOG_CATCH_ALL()
#else
#define SPDLOG_TRY try
#define SPDLOG_THROW(ex) throw(ex)
#define SPDLOG_CATCH_ALL() catch (...)
#endif

namespace spdlog {

class formatter;

namespace sinks {
class sink;
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
using filename_t = std::wstring;
// allow macro expansion to occur in SPDLOG_FILENAME_T
#define SPDLOG_FILENAME_T_INNER(s) L##s
#define SPDLOG_FILENAME_T(s) SPDLOG_FILENAME_T_INNER(s)
#else
using filename_t = std::string;
#define SPDLOG_FILENAME_T(s) s
#endif

/*
*   当用作字符串化操作时，#的主要作用是将宏参数不经扩展地转换成字符串常量。
*   #就是将参数变成字符串
*   ## 则是连接作用
*/

using log_clock = std::chrono::system_clock;
using sink_ptr = std::shared_ptr<sinks::sink>;
using sinks_init_list = std::initializer_list<sink_ptr>;
using err_handler = std::function<void(const std::string &err_msg)>;
using string_view_t = fmt::basic_string_view<char>;
using wstring_view_t = fmt::basic_string_view<wchar_t>;
using memory_buf_t = fmt::basic_memory_buffer<char, 250>;
using wmemory_buf_t = fmt::basic_memory_buffer<wchar_t, 250>;

#ifdef SPDLOG_WCHAR_TO_UTF8_SUPPORT
#ifndef _WIN32
#error SPDLOG_WCHAR_TO_UTF8_SUPPORT only supported on windows
#else
template<typename T>
struct is_convertible_to_wstring_view : std::is_convertible<T, wstring_view_t>
{};
#endif // _WIN32
#else
template<typename>
struct is_convertible_to_wstring_view : std::false_type
{};
#endif // SPDLOG_WCHAR_TO_UTF8_SUPPORT

#if defined(SPDLOG_NO_ATOMIC_LEVELS)
using level_t = details::null_atomic_int;
#else
using level_t = std::atomic<int>;
#endif

/*
*   C++中对共享数据的存取在并发条件下可能会引起data race的undifined行为，需要限制并发程序以某种特定的顺序执行，
*   有两种方式：使用mutex保护共享数据，原子操作.
*   原子操作:针对原子类型操作要不一步完成，要么不做，不可能出现操作一半被切换CPU.
*   这样防止由于多线程指令交叉执行带来的可能错误,因为：非原子操作下，某个线程可能看见的是一个其它线程操作未完成的数据。
*/

#define SPDLOG_LEVEL_TRACE 0
#define SPDLOG_LEVEL_DEBUG 1
#define SPDLOG_LEVEL_INFO 2
#define SPDLOG_LEVEL_WARN 3
#define SPDLOG_LEVEL_ERROR 4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF 6

#if !defined(SPDLOG_ACTIVE_LEVEL)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

// Log level enum
namespace level {
enum level_enum
{
    trace = SPDLOG_LEVEL_TRACE,
    debug = SPDLOG_LEVEL_DEBUG,
    info = SPDLOG_LEVEL_INFO,
    warn = SPDLOG_LEVEL_WARN,
    err = SPDLOG_LEVEL_ERROR,
    critical = SPDLOG_LEVEL_CRITICAL,
    off = SPDLOG_LEVEL_OFF,
    n_levels
};

#if !defined(SPDLOG_LEVEL_NAMES)
#define SPDLOG_LEVEL_NAMES                                                                                                                 \
    {                                                                                                                                      \
        "trace", "debug", "info", "warning", "error", "critical", "off"                                                                    \
    }
#endif

#if !defined(SPDLOG_SHORT_LEVEL_NAMES)

#define SPDLOG_SHORT_LEVEL_NAMES                                                                                                           \
    {                                                                                                                                      \
        "T", "D", "I", "W", "E", "C", "O"                                                                                                  \
    }
#endif

SPDLOG_API const string_view_t &to_string_view(spdlog::level::level_enum l) SPDLOG_NOEXCEPT;
SPDLOG_API void set_string_view(spdlog::level::level_enum l, const string_view_t &s) SPDLOG_NOEXCEPT;
SPDLOG_API const char *to_short_c_str(spdlog::level::level_enum l) SPDLOG_NOEXCEPT;
SPDLOG_API spdlog::level::level_enum from_str(const std::string &name) SPDLOG_NOEXCEPT;

/*
*   哈希是用给定范围的基本类型的数据项，或者用像 string 这样的对象，生成整数值的过程。
*   哈希产生的值叫作哈希值或哈希码，它们通常被用在容器中，用来确定表中对象的位置。
*   像前面所说的那样，理想情况下，每个对象应该产生唯一的哈希值，但这一般是不可能的。
*   当不同键值的个数大于可能的哈希值个数时，显然就会出现上面所说的这种情况，我们早晚会得到重复的哈希值。
*   重复的哈希值也叫作碰撞。
*/

} // namespace level

//
// Color mode used by sinks with color support.
//
enum class color_mode
{
    always,
    automatic,
    never
};

//
// Pattern time - specific time getting to use for pattern_formatter.
// local time by default
//
enum class pattern_time_type
{
    local, // log localtime
    utc    // log utc
};

//
// Log exception 异常处理
//
class SPDLOG_API spdlog_ex : public std::exception
{
public:
    explicit spdlog_ex(std::string msg);
    spdlog_ex(const std::string &msg, int last_errno);
    const char *what() const SPDLOG_NOEXCEPT override;

private:
    std::string msg_;
};

[[noreturn]] SPDLOG_API void throw_spdlog_ex(const std::string &msg, int last_errno);
[[noreturn]] SPDLOG_API void throw_spdlog_ex(std::string msg);

struct source_loc
{
    SPDLOG_CONSTEXPR source_loc() = default;
    SPDLOG_CONSTEXPR source_loc(const char *filename_in, int line_in, const char *funcname_in)
        : filename{filename_in}
        , line{line_in}
        , funcname{funcname_in}
    {}

    SPDLOG_CONSTEXPR bool empty() const SPDLOG_NOEXCEPT
    {
        return line == 0;
    }
    const char *filename{nullptr};
    int line{0};
    const char *funcname{nullptr};
};

namespace details {
// make_unique support for pre c++14

#if __cplusplus >= 201402L // C++14 and beyond
using std::make_unique;
#else
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args)
{
    static_assert(!std::is_array<T>::value, "arrays not supported");
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif
} // namespace details
} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "common-inl.h"
#endif
