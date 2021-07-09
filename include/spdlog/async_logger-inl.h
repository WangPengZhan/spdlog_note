// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/async_logger.h>
#endif

#include <spdlog/sinks/sink.h>
#include <spdlog/details/thread_pool.h>

#include <memory>
#include <string>

SPDLOG_INLINE spdlog::async_logger::async_logger(
    std::string logger_name, sinks_init_list sinks_list, std::weak_ptr<details::thread_pool> tp, async_overflow_policy overflow_policy)
    : async_logger(std::move(logger_name), sinks_list.begin(), sinks_list.end(), std::move(tp), overflow_policy)
{}

SPDLOG_INLINE spdlog::async_logger::async_logger(
    std::string logger_name, sink_ptr single_sink, std::weak_ptr<details::thread_pool> tp, async_overflow_policy overflow_policy)
    : async_logger(std::move(logger_name), {std::move(single_sink)}, std::move(tp), overflow_policy)
{}

// send the log message to the thread pool
SPDLOG_INLINE void spdlog::async_logger::sink_it_(const details::log_msg &msg)
{
    if (auto pool_ptr = thread_pool_.lock())
    {
        pool_ptr->post_log(shared_from_this(), msg, overflow_policy_);
    }
    else
    {
        throw_spdlog_ex("async log: thread pool doesn't exist anymore");
    }
}

// send flush request to the thread pool
SPDLOG_INLINE void spdlog::async_logger::flush_()
{
    /*  weak_ptr.lock()
     *  weak_ptr被设计为与shared_ptr共同工作，可以从一个shared_ptr或者另一个weak_ptr对象构造，获得资源的观测权。
     *  但weak_ptr没有共享资源，它的构造不会引起指针引用计数的增加。同样，在weak_ptr析构时也不会导致引用计数的减少，它只是一个静静地观察者。
     *  weak_ptr没有重载operator*和->，这是特意的，因为它不共享指针，不能操作资源，这是它弱的原因。
     *  但它可以使用一个非常重要的成员函数lock()从被观测的shared_ptr获得一个可用的shared_ptr对象，从而操作资源。
     *  weak_ptr用于解决”引用计数”模型循环依赖问题，weak_ptr指向一个对象，并不增减该对象的引用计数器。
     *  weak_ptr用于配合shared_ptr使用，并不影响动态对象的生命周期，即其存在与否并不影响对象的引用计数器。
     *  weak_ptr提供了expired()与lock()成员函数，
     *  前者用于判断weak_ptr指向的对象是否已被销毁，后者返回其所指对象的shared_ptr智能指针(对象销毁时返回”空”shared_ptr)。
     */

    /*
    * shared_from_this()的用途
    * enable_shared_from_this是一个模板类，定义于头文件<memory>，其原型为：
    * template< class T > class enable_shared_from_this;
    * std::enable_shared_from_this 能让一个对象（假设其名为 t ，且已被一个 std::shared_ptr 对象 pt 管理）
    * 安全地生成其他额外的 std::shared_ptr 实例（假设名为 pt1, pt2, ... ） ，它们与 pt 共享对象 t 的所有权。
    * 若一个类 T 继承 std::enable_shared_from_this<T> ，则会为该类 T 提供成员函数： shared_from_this 。
    * 当 T 类型对象 t 被一个为名为 pt 的 std::shared_ptr<T> 类对象管理时，调用 T::shared_from_this 成员函数，
    * 将会返回一个新的 std::shared_ptr<T> 对象，它与 pt 共享 t 的所有权。
    * (1)使用原因
     1.把当前类对象作为参数传给其他函数时，为什么要传递share_ptr呢？直接传递this指针不可以吗？
         一个裸指针传递给调用者，谁也不知道调用者会干什么？假如调用者delete了该对象，而share_tr此时还指向该对象。

     2.这样传递share_ptr可以吗？share_ptr <this>
         这样会造成2个非共享的share_ptr指向一个对象，最后造成2次析构该对象。
    * (2)实现原理
     enable_shared_from_this的一种实现方法是，其内部有一个weak_ptr类型的成员变量_Wptr，
     当shared_ptr构造的时候，如果其模板类型继承了enable_shared_from_this，则对_Wptr进行初始化操作，
     这样将来调用shared_from_this函数的时候，就能够通过weak_ptr构造出对应的shared_ptr。
    */

    if (auto pool_ptr = thread_pool_.lock())
    {
        pool_ptr->post_flush(shared_from_this(), overflow_policy_);
    }
    else
    {
        throw_spdlog_ex("async flush: thread pool doesn't exist anymore");
    }
}

//
// backend functions - called from the thread pool to do the actual job
//
SPDLOG_INLINE void spdlog::async_logger::backend_sink_it_(const details::log_msg &msg)
{
    for (auto &sink : sinks_)
    {
        if (sink->should_log(msg.level))
        {
            SPDLOG_TRY
            {
                sink->log(msg);
            }
            SPDLOG_LOGGER_CATCH()
        }
    }

    if (should_flush_(msg))
    {
        backend_flush_();
    }
}

SPDLOG_INLINE void spdlog::async_logger::backend_flush_()
{
    for (auto &sink : sinks_)
    {
        SPDLOG_TRY
        {
            sink->flush();
        }
        SPDLOG_LOGGER_CATCH()
    }
}

SPDLOG_INLINE std::shared_ptr<spdlog::logger> spdlog::async_logger::clone(std::string new_name)
{
    auto cloned = std::make_shared<spdlog::async_logger>(*this);
    cloned->name_ = std::move(new_name);
    return cloned;
}
