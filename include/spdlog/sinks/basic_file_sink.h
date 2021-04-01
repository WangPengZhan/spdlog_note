// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/synchronous_factory.h>

#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {
/*
 * Trivial file sink with single file as target
 */
template<typename Mutex>
class basic_file_sink final : public base_sink<Mutex>
{
public:
    explicit basic_file_sink(const filename_t &filename, bool truncate = false);
    const filename_t &filename() const;
    /*
    *   const 函数不同位置的意义
    *   如果关键字const出现在*之前表示所指之物是常量，出现在*之后表示指针本身是常量
    *   const char* const Func(const int& a) const;
    *   (1) const 表示返回值所指之物是常量
    *   (2) const 表示返回指针本身是常量
    *   (3) const 表示传入参数为常量，仅对指针和引用有意义
    *   (4) const 表示不改变类成员变量
    *
    */

protected:
    void sink_it_(const details::log_msg &msg) override;
    void flush_() override;

private:
    details::file_helper file_helper_;
};

using basic_file_sink_mt = basic_file_sink<std::mutex>;
using basic_file_sink_st = basic_file_sink<details::null_mutex>;

} // namespace sinks

// template防止歧义，采用模板类里面的模板函数时.向编译器指明时模板函数而不是成员变量
// 可以在相应的类所在文件的namespace重载函数

//
// factory functions
//
template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_mt(const std::string &logger_name, const filename_t &filename, bool truncate = false)
{
    return Factory::template create<sinks::basic_file_sink_mt>(logger_name, filename, truncate);
}

template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_st(const std::string &logger_name, const filename_t &filename, bool truncate = false)
{
    return Factory::template create<sinks::basic_file_sink_st>(logger_name, filename, truncate);
}

} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "basic_file_sink-inl.h"
#endif