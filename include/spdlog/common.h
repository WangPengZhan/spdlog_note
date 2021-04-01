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
#define SPDLOG_API    // SPDLOG APIΪ���ַ���
#endif
#define SPDLOG_INLINE
#else // !defined(SPDLOG_COMPILED_LIB)
#define SPDLOG_API
#define SPDLOG_HEADER_ONLY
#define SPDLOG_INLINE inline
#endif // #ifdef SPDLOG_COMPILED_LIB

/*
*   _declspec��Microsoft VC��ר�õĹؼ��֣��������һЩ���Կ��ԶԱ�׼C/C++�������䡣
*   __declspec�ؼ���Ӧ�ó�����������ǰ�档
*
*   __declspec(dllexport)����Windows�еĶ�̬���У����������������ࡢ����ȹ�������ã�ʡ�Ը���.def�ļ���
*   �����������������Ϊ����������������������ã���Ϊ��̬��Ķ���ӿں�������ȡ�

*   .def�ļ�(ģ�鶨���ļ�)�ǰ���һ��������������DLL���Ե�Module�����ı��ļ���
*   .def�ļ���__declspec(dllexport)���ǽ��������ŵ��뵽Ӧ�ó�����DLL����������
*   ������ṩ__declspec(dllexport)����DLL��������DLL��Ҫ�ṩ.def�ļ���

*   __declspec(dllimport)����Windows�У��ӱ�Ķ�̬�����������뺯�����ࡢ����ȹ�����̬���exe�ļ�ʹ�á�
*   ������Ҫʹ��DLL�еĺ���ʱ����������Ҫ��ʾ�ص��뺯�������������Զ���ɡ�
*   ��ʹ��__declspec(dllimport)Ҳ����ȷ������룬��ʹ��__declspec(dllimport)ʹ�������������ɸ��õĴ��롣
*   ������֮�����ܹ����ɸ��õĴ��룬����Ϊ������ȷ�������Ƿ������DLL�У���ʹ�ñ��������������������Ѱַ����Ĵ��룬
*   ����Щ����ͨ��������ڿ�DLL�߽�ĺ��������С�
*   ����һ�����뺯������˵��������Ǵӱ��DLL���롣һ������ʹ��ĳ��DLL��exe�С�
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
#define SPDLOG_DEPRECATED __attribute__((deprecated)) //��__attribute__((deprecated)) �����ʱ���룬ͬʱ�������ݵĽӿ�
#elif defined(_MSC_VER)
#define SPDLOG_DEPRECATED __declspec(deprecated)
#else
#define SPDLOG_DEPRECATED
#endif

/*
*   �ڿ���һЩ���ʱ��API�Ľӿڿ��ܻ��ʱ��Ϊ�����ѿ�������������Ѿ���ʱ��
*   �����ں�������ʱ����attribute((deprecated))���ԣ���ˣ�
*   ֻҪ������ʹ�ã��ڱ���ʱ����������棬������Ϣ�а�����ʱ�ӿڵ����Ƽ������е�����λ�á�
*
*   attribute�������ú������ԣ�Function Attribute�����������ԣ�Variable Attribute�����������ԣ�Type Attribute����
*   attribute�﷨��ʽΪ��attribute ((attribute))
*   ע�⣺ ʹ��attribute��ʱ��ֻ�ܺ�����������ʹ��attribute��
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
*   �������ַ���������ʱ��#����Ҫ�����ǽ������������չ��ת�����ַ���������
*   #���ǽ���������ַ���
*   ## ������������
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
*   C++�жԹ������ݵĴ�ȡ�ڲ��������¿��ܻ�����data race��undifined��Ϊ����Ҫ���Ʋ���������ĳ���ض���˳��ִ�У�
*   �����ַ�ʽ��ʹ��mutex�����������ݣ�ԭ�Ӳ���.
*   ԭ�Ӳ���:���ԭ�����Ͳ���Ҫ��һ����ɣ�Ҫô�����������ܳ��ֲ���һ�뱻�л�CPU.
*   ������ֹ���ڶ��߳�ָ���ִ�д����Ŀ��ܴ���,��Ϊ����ԭ�Ӳ����£�ĳ���߳̿��ܿ�������һ�������̲߳���δ��ɵ����ݡ�
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
*   ��ϣ���ø�����Χ�Ļ������͵�������������� string �����Ķ�����������ֵ�Ĺ��̡�
*   ��ϣ������ֵ������ϣֵ���ϣ�룬����ͨ�������������У�����ȷ�����ж����λ�á�
*   ��ǰ����˵����������������£�ÿ������Ӧ�ò���Ψһ�Ĺ�ϣֵ������һ���ǲ����ܵġ�
*   ����ͬ��ֵ�ĸ������ڿ��ܵĹ�ϣֵ����ʱ����Ȼ�ͻ����������˵��������������������õ��ظ��Ĺ�ϣֵ��
*   �ظ��Ĺ�ϣֵҲ������ײ��
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
// Log exception �쳣����
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
