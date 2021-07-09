// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once
//
// base sink templated over a mutex (either dummy or real)
// concrete implementation should override the sink_it_() and flush_()  methods.
// locking is taken care of in this class - no locking needed by the
// implementers..
//

#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/sink.h>

namespace spdlog {
namespace sinks {
template<typename Mutex>
class base_sink : public sink
{
public:
    base_sink();
    explicit base_sink(std::unique_ptr<spdlog::formatter> formatter);  // explicit 构造函数的类，不能发生相应的隐式类型转换，只能以显示的方式进行类型转换
    ~base_sink() override = default;                                   // override 可以避免派生类中忘记重写虚函数的错误

    base_sink(const base_sink &) = delete;                             // delete 可在想要 “禁止使用” 的特殊成员函数声明后加 “= delete”
    base_sink(base_sink &&) = delete;                                  // 带有右值的构造函数 移动构造函数

    base_sink &operator=(const base_sink &) = delete;                  // 赋值构造函数
    base_sink &operator=(base_sink &&) = delete;

    void log(const details::log_msg &msg) final;                       // final 禁用重写
    void flush() final;
    void set_pattern(const std::string &pattern) final;                // 相当于模板方法 加锁 方便以后不用重复
    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) final;

protected:
    // sink formatter
    std::unique_ptr<spdlog::formatter> formatter_;
    Mutex mutex_;                                                      // 为了同步多线程

    virtual void sink_it_(const details::log_msg &msg) = 0;
    virtual void flush_() = 0;
    virtual void set_pattern_(const std::string &pattern);
    virtual void set_formatter_(std::unique_ptr<spdlog::formatter> sink_formatter);
};
} // namespace sinks
} // namespace spdlog

// 模板类的声明和实现要放在一起
#ifdef SPDLOG_HEADER_ONLY
#include "base_sink-inl.h"
#endif
