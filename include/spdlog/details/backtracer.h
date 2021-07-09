// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/log_msg_buffer.h>
#include <spdlog/details/circular_q.h>

#include <atomic>
#include <mutex>
#include <functional>

// Store log messages in circular buffer.
// Useful for storing debug data in case of error/warning happens.

namespace spdlog {
namespace details {
class SPDLOG_API backtracer
{
    mutable std::mutex mutex_;
    // 在C++中，mutable是为了突破const的限制而设置的。被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中。
    std::atomic<bool> enabled_{false};
    // C++中对共享数据的存取在并发条件下可能会引起data race的undifined行为，需要限制并发程序以某种特定的顺序执行，
    // 有两种方式：使用mutex保护共享数据，原子操作：针对原子类型操作要不一步完成，要么不做，不可能出现操作一半被切换CPU，
    // 这样防止由于多线程指令交叉执行带来的可能错误。非原子操作下，某个线程可能看见的是一个其它线程操作未完成的数据。
    circular_q<log_msg_buffer> messages_;

public:
    backtracer() = default;
    backtracer(const backtracer &other);

    backtracer(backtracer &&other) SPDLOG_NOEXCEPT;
    backtracer &operator=(backtracer other);

    void enable(size_t size);
    void disable();
    bool enabled() const;
    void push_back(const log_msg &msg);

    // pop all items in the q and apply the given fun on each of them.
    void foreach_pop(std::function<void(const details::log_msg &)> fun);
};

} // namespace details
} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "backtracer-inl.h"
#endif