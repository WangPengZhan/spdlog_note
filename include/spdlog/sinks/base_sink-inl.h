// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// 技巧
#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/base_sink.h>
#endif

#include <spdlog/common.h>
#include <spdlog/pattern_formatter.h>

#include <memory>
/*
*   1. unique_ptr
*   unique_ptr是个独占指针，unique_ptr所指的内存为自己独有，
*   某个时刻只能有一个unique_ptr指向一个给定的对象，不支持拷贝和赋值。
*   支持std::move进行拥有权转移
*   释放方式:(1)reset() (2)reset(new objection) (3)ptr = nullptr (4) std::move(ptr)
*   release只释放管理权，而不释放内存
* 
*   2 shared_ptr
*   实现的方式: 引用计数 指向动态分配的int
*   构造方式: make_shared<typename(arg...)>
* 
*   3.weak_ptr
*   weak_ptr是为配合shared_ptr而引入的一种智能指针来协助shared_ptr工作，双拥有闭环
*   它可以从一个shared_ptr或另一个weak_ptr对象构造，它的构造和析构不会引起引用计数的增加或减少。
*   没有重载 *和 -> 但可以使用lock获得一个可用的shared_ptr对象
*   weak_ptr的使用更为复杂一点，它可以指向shared_ptr指针指向的对象内存，却并不拥有该内存，
*   而使用weak_ptr成员lock，则可返回其指向内存的一个share_ptr对象，且在所指对象内存已经无效时，返回指针空值nullptr。
*   注意：weak_ptr并不拥有资源的所有权，所以不能直接使用资源。 可以从一个weak_ptr构造一个shared_ptr以取得共享资源的所有权。
*/

template<typename Mutex>
SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::base_sink()
    : formatter_{details::make_unique<spdlog::pattern_formatter>()}
{}

template<typename Mutex>
SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::base_sink(std::unique_ptr<spdlog::formatter> formatter)
    : formatter_{std::move(formatter)}
{}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::log(const details::log_msg &msg)
{
    std::lock_guard<Mutex> lock(mutex_);
    sink_it_(msg);
}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::flush()
{
    std::lock_guard<Mutex> lock(mutex_);
    flush_();
}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::set_pattern(const std::string &pattern)
{
    std::lock_guard<Mutex> lock(mutex_);
    set_pattern_(pattern);
}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter)
{
    std::lock_guard<Mutex> lock(mutex_);
    set_formatter_(std::move(sink_formatter));
}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::set_pattern_(const std::string &pattern)
{
    set_formatter_(details::make_unique<spdlog::pattern_formatter>(pattern));
}

template<typename Mutex>
void SPDLOG_INLINE spdlog::sinks::base_sink<Mutex>::set_formatter_(std::unique_ptr<spdlog::formatter> sink_formatter)
{
    formatter_ = std::move(sink_formatter);
}
/*
*   1.lock_guard
*   (1)
*   互斥类的最重要成员函数是lock()和unlock()。
*   在进入临界区时，执行lock()加锁操作，如果这时已经被其它线程锁住，则当前线程在此排队等待。
*   退出临界区时，执行unlock()解锁操作。
*   (2)
*   更好的办法是采用”资源分配时初始化”(RAII)方法来加锁、解锁，这避免了在临界区中因为抛出异常或return等操作导致没有解锁就退出的问题。
*   C++11的标准库中提供了std::lock_guard类模板做mutex的RAII。
*   (3)
*   std::lock_guard类的构造函数禁用拷贝构造，且禁用移动构造。
*   std::lock_guard类除了构造函数和析构函数外没有其它成员函数。
*   在std::lock_guard对象构造时，传入的mutex对象(即它所管理的mutex对象)会被当前线程锁住。
*   在lock_guard对象被析构时，它所管理的mutex对象会自动解锁，不需要程序员手动调用lock和unlock对mutex进行上锁和解锁操作。
*   lock_guard对象并不负责管理mutex对象的生命周期，lock_guard对象只是简化了mutex对象的上锁和解锁操作，方便线程对互斥量上锁，
*   即在某个lock_guard对象的生命周期内，它所管理的锁对象会一直保持上锁状态；
*   而lock_guard的生命周期结束之后，它所管理的锁对象会被解锁。
*   程序员可以非常方便地使用lock_guard，而不用担心异常安全问题。
*   std::lock_guard在构造时只被锁定一次，并且在销毁时解锁。
* 
*/
