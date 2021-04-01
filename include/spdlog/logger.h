// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// Thread safe logger (except for set_error_handler())
// Has name, log level, vector of std::shared sink pointers and formatter
// Upon each log write the logger:
// 1. Checks if its log level is enough to log the message and if yes:
// 2. Call the underlying sinks to do the job.
// 3. Each sink use its own private copy of a formatter to format the message
// and send to its destination.
//
// The use of private formatter per sink provides the opportunity to cache some
// formatted data, and support for different format per sink.

#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/backtracer.h>

#ifdef SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <spdlog/details/os.h>
#endif

#include <vector>
#ifndef SPDLOG_NO_EXCEPTIONS
// 异常处理
#define SPDLOG_LOGGER_CATCH()                                                                                                              \
    catch (const std::exception &ex)                                                                                                       \
    {                                                                                                                                      \
        err_handler_(ex.what());                                                                                                           \
    }                                                                                                                                      \
    catch (...)                                                                                                                            \
    {                                                                                                                                      \
        err_handler_("Unknown exception in logger");                                                                                       \
    }
#else
#define SPDLOG_LOGGER_CATCH()
#endif

namespace spdlog {

class SPDLOG_API logger
{
public:
    // 构造函数重载
    // Empty logger
    explicit logger(std::string name)
        : name_(std::move(name))
        , sinks_()
    {}

    /*
    * std::move
    * std::move并不能移动任何东西。
    * 它唯一的功能是将一个左值引用强制转化为右值引用，继而可以通过右值引用使用该值，以用于移动语义。
    * 从实现上讲，std::move基本等同于一个类型转换：static_cast<T&&>(lvalue);
    * std::forword()
    * 完美转发实现了参数在传递过程中保持其值属性的功能，
    * 即若是左值，则传递之后仍然是左值，若是右值，则传递之后仍然是右值。
    */

    // Logger with range on sinks
    template<typename It>
    logger(std::string name, It begin, It end)
        : name_(std::move(name))
        , sinks_(begin, end)
    {}

    // Logger with single sink
    logger(std::string name, sink_ptr single_sink)
        : logger(std::move(name), {std::move(single_sink)})
    {}

    // Logger with sinks init list
    logger(std::string name, sinks_init_list sinks)
        : logger(std::move(name), sinks.begin(), sinks.end())
    {}

    virtual ~logger() = default;

    // 拷贝构造函数
    logger(const logger &other);

    // 移动构造函数
    logger(logger &&other) SPDLOG_NOEXCEPT;

    // 移动构造函数
    logger &operator=(logger other) SPDLOG_NOEXCEPT;

    // 交换操作
    void swap(spdlog::logger &other) SPDLOG_NOEXCEPT;

    // 全版本 转交给log_
    // FormatString is a type derived from fmt::compile_string
    // fmt::is_compile_string<FormatString>::value // 判断是不是fmt里面可以编译的字符串
    // true 第二个参数为int类型 默认值为 0
    // false 第二个参数为void 发生一个编译错误
    // 作用：控制接收数据类型
    template<typename FormatString, typename std::enable_if<fmt::is_compile_string<FormatString>::value, int>::type = 0, typename... Args>
    void log(source_loc loc, level::level_enum lvl, const FormatString &fmt, Args&&...args)
    {
        log_(loc, lvl, fmt, std::forward<Args>(args)...);
    }

    // FormatString is NOT a type derived from fmt::compile_string but is a string_view_t or can be implicitly converted to one
    template<typename... Args>
    void log(source_loc loc, level::level_enum lvl, string_view_t fmt, Args&&...args)
    {
        log_(loc, lvl, fmt, std::forward<Args>(args)...);
    }

    // 转交上方函数
    template<typename FormatString, typename... Args>
    void log(level::level_enum lvl, const FormatString &fmt, Args&&...args)
    {
        log(source_loc{}, lvl, fmt, std::forward<Args>(args)...);
    }

    // 根据level不同提供不同的接口 转交给上方函数
    template<typename FormatString, typename... Args>
    void trace(const FormatString &fmt, Args&&...args)
    {
        log(level::trace, fmt, std::forward<Args>(args)...);
    }

    template<typename FormatString, typename... Args>
    void debug(const FormatString &fmt, Args&&...args)
    {
        log(level::debug, fmt, std::forward<Args>(args)...);
    }

    template<typename FormatString, typename... Args>
    void info(const FormatString &fmt, Args&&...args)
    {
        log(level::info, fmt, std::forward<Args>(args)...);
    }

    template<typename FormatString, typename... Args>
    void warn(const FormatString &fmt, Args&&...args)
    {
        log(level::warn, fmt, std::forward<Args>(args)...);
    }

    template<typename FormatString, typename... Args>
    void error(const FormatString &fmt, Args&&...args)
    {
        log(level::err, fmt, std::forward<Args>(args)...);
    }

    template<typename FormatString, typename... Args>
    void critical(const FormatString &fmt, Args&&...args)
    {
        log(level::critical, fmt, std::forward<Args>(args)...);
    }

    // msg T
    template<typename T>
    void log(level::level_enum lvl, const T &msg)
    {
        log(source_loc{}, lvl, msg);
    }

    // std::is_convertible<> 
    // T can be statically converted to string_view and isn't a fmt::compile_string
    template<class T, typename std::enable_if<
                          std::is_convertible<const T &, spdlog::string_view_t>::value && !fmt::is_compile_string<T>::value, int>::type = 0>
    void log(source_loc loc, level::level_enum lvl, const T &msg)
    {
        log(loc, lvl, string_view_t{msg});
    }

    void log(log_clock::time_point log_time, source_loc loc, level::level_enum lvl, string_view_t msg)
    {
        bool log_enabled = should_log(lvl);
        bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled)
        {
            return;
        }

        details::log_msg log_msg(log_time, loc, name_, lvl, msg);
        log_it_(log_msg, log_enabled, traceback_enabled);
    }

    void log(source_loc loc, level::level_enum lvl, string_view_t msg)
    {
        bool log_enabled = should_log(lvl);
        bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled)
        {
            return;
        }

        details::log_msg log_msg(loc, name_, lvl, msg);
        log_it_(log_msg, log_enabled, traceback_enabled);
    }

    void log(level::level_enum lvl, string_view_t msg)
    {
        log(source_loc{}, lvl, msg);
    }

    // T cannot be statically converted to string_view or wstring_view
    template<class T, typename std::enable_if<!std::is_convertible<const T &, spdlog::string_view_t>::value &&
                                                  !is_convertible_to_wstring_view<const T &>::value,
                          int>::type = 0>
    void log(source_loc loc, level::level_enum lvl, const T &msg)
    {
        log(loc, lvl, "{}", msg);
    }

    template<typename T>
    void trace(const T &msg)
    {
        log(level::trace, msg);
    }

    template<typename T>
    void debug(const T &msg)
    {
        log(level::debug, msg);
    }

    template<typename T>
    void info(const T &msg)
    {
        log(level::info, msg);
    }

    template<typename T>
    void warn(const T &msg)
    {
        log(level::warn, msg);
    }

    template<typename T>
    void error(const T &msg)
    {
        log(level::err, msg);
    }

    template<typename T>
    void critical(const T &msg)
    {
        log(level::critical, msg);
    }

#ifdef SPDLOG_WCHAR_TO_UTF8_SUPPORT
#ifndef _WIN32
#error SPDLOG_WCHAR_TO_UTF8_SUPPORT only supported on windows
#else

    template<typename... Args>
    void log(source_loc loc, level::level_enum lvl, wstring_view_t fmt, Args&&...args)
    {
        bool log_enabled = should_log(lvl);
        bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled)
        {
            return;
        }
        SPDLOG_TRY
        {
            // format to wmemory_buffer and convert to utf8
            fmt::wmemory_buffer wbuf;
            fmt::format_to(wbuf, fmt, std::forward<Args>(args)...);

            memory_buf_t buf;
            details::os::wstr_to_utf8buf(wstring_view_t(wbuf.data(), wbuf.size()), buf);
            details::log_msg log_msg(loc, name_, lvl, string_view_t(buf.data(), buf.size()));
            log_it_(log_msg, log_enabled, traceback_enabled);
        }
        SPDLOG_LOGGER_CATCH()
    }

    // T can be statically converted to wstring_view
    template<class T, typename std::enable_if<is_convertible_to_wstring_view<const T &>::value, int>::type = 0>
    void log(source_loc loc, level::level_enum lvl, const T &msg)
    {
        bool log_enabled = should_log(lvl);
        bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled)
        {
            return;
        }

        SPDLOG_TRY
        {
            memory_buf_t buf;
            details::os::wstr_to_utf8buf(msg, buf);
            details::log_msg log_msg(loc, name_, lvl, string_view_t(buf.data(), buf.size()));
            log_it_(log_msg, log_enabled, traceback_enabled);
        }
        SPDLOG_LOGGER_CATCH()
    }
#endif // _WIN32
#endif // SPDLOG_WCHAR_TO_UTF8_SUPPORT

    // return true logging is enabled for the given level.
    bool should_log(level::level_enum msg_level) const
    {
        return msg_level >= level_.load(std::memory_order_relaxed);
    }

    // return true if backtrace logging is enabled.
    bool should_backtrace() const
    {
        return tracer_.enabled();
    }

    void set_level(level::level_enum log_level);

    level::level_enum level() const;

    const std::string &name() const;

    // set formatting for the sinks in this logger.
    // each sink will get a separate instance of the formatter object.
    void set_formatter(std::unique_ptr<formatter> f);

    void set_pattern(std::string pattern, pattern_time_type time_type = pattern_time_type::local);

    // backtrace support.
    // efficiently store all debug/trace messages in a circular buffer until needed for debugging.
    void enable_backtrace(size_t n_messages);
    void disable_backtrace();
    void dump_backtrace();

    // flush functions
    void flush();
    void flush_on(level::level_enum log_level);
    level::level_enum flush_level() const;

    // sinks
    const std::vector<sink_ptr> &sinks() const;
    std::vector<sink_ptr> &sinks();
    // const 函数重载

    // error handler
    void set_error_handler(err_handler);

    // create new logger with same sinks and configuration.
    virtual std::shared_ptr<logger> clone(std::string logger_name);

protected:
    std::string name_;
    std::vector<sink_ptr> sinks_;
    spdlog::level_t level_{level::info};
    spdlog::level_t flush_level_{level::off};
    err_handler custom_err_handler_{nullptr}; // 异常函数
    details::backtracer tracer_;
   

    // common implementation for after templated public api has been resolved
    template<typename FormatString, typename... Args>
    void log_(source_loc loc, level::level_enum lvl, const FormatString &fmt, Args&&...args)
    {
        bool log_enabled = should_log(lvl);
        bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled)
        {
            return;
        }
        SPDLOG_TRY
        {
            memory_buf_t buf;
            fmt::format_to(buf, fmt, std::forward<Args>(args)...);
            details::log_msg log_msg(loc, name_, lvl, string_view_t(buf.data(), buf.size()));
            log_it_(log_msg, log_enabled, traceback_enabled);
        }
        SPDLOG_LOGGER_CATCH()
    }

    // log the given message (if the given log level is high enough),
    // and save backtrace (if backtrace is enabled).
    void log_it_(const details::log_msg &log_msg, bool log_enabled, bool traceback_enabled);
    virtual void sink_it_(const details::log_msg &msg);
    virtual void flush_();
    void dump_backtrace_();
    bool should_flush_(const details::log_msg &msg);

    // handle errors during logging.
    // default handler prints the error to stderr at max rate of 1 message/sec.
    void err_handler_(const std::string &msg);
};

void swap(logger &a, logger &b);

} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "logger-inl.h"
#endif

/*
 * C++中每一个对象所占用的空间大小，是在编译的时候就确定的.
 * 在模板类没有真正的被使用之前，编译器是无法知道，模板类中使用模板类型的对象的所占用的空间的大小的。
 * 只有模板被真正使用的时候,比如A<int> , A<double >，编译器才知道，模板套用的是什么类型，应该分配多少空间。
 * 套用不同类型的模板类实际上就是两个不同的类型，
 * 也就是说，stack<int>和stack<char>是两个不同的数据类型，
 * 他们共同的成员函数也不是同一个函数，只不过具有相似的功能罢了。
 *
 */
