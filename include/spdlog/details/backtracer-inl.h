// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/details/backtracer.h>
#endif
namespace spdlog {
namespace details {
SPDLOG_INLINE backtracer::backtracer(const backtracer &other)
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    enabled_ = other.enabled();
    messages_ = other.messages_;
}

SPDLOG_INLINE backtracer::backtracer(backtracer &&other) SPDLOG_NOEXCEPT
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    enabled_ = other.enabled();
    messages_ = std::move(other.messages_);
}

SPDLOG_INLINE backtracer &backtracer::operator=(backtracer other)
{
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = other.enabled();
    messages_ = std::move(other.messages_);
    return *this;
}

SPDLOG_INLINE void backtracer::enable(size_t size)
{
    std::lock_guard<std::mutex> lock{mutex_};
    enabled_.store(true, std::memory_order_relaxed);
    messages_ = circular_q<log_msg_buffer>{size};
}

SPDLOG_INLINE void backtracer::disable()
{
    std::lock_guard<std::mutex> lock{mutex_};
    enabled_.store(false, std::memory_order_relaxed);
}

SPDLOG_INLINE bool backtracer::enabled() const
{
    return enabled_.load(std::memory_order_relaxed);
}

SPDLOG_INLINE void backtracer::push_back(const log_msg &msg)
{
    std::lock_guard<std::mutex> lock{mutex_};
    messages_.push_back(log_msg_buffer{msg});
}

// pop all items in the q and apply the given fun on each of them.
SPDLOG_INLINE void backtracer::foreach_pop(std::function<void(const details::log_msg &)> fun)
{
    std::lock_guard<std::mutex> lock{mutex_};
    while (!messages_.empty())
    {
        auto &front_msg = messages_.front();
        fun(front_msg);
        messages_.pop_front();
    }
}
} // namespace details
} // namespace spdlog


/*
*   atomic<T> 模板类
*   其中T是trivially copyable type满足：
*   1.要么全部定义了拷贝/移动/赋值函数，要么全部没定义;
*   2.没有虚成员;
*   3.基类或其它任何非static成员都是trivally copyable
*   典型的内置类型bool、int等属于trivally copyable。
*   再如class triviall{public : int x};也是。
*   T能够被memcpy、memcmp函数使用，从而支持compare/exchange系列函数。
*   有一条规则：不要在保护数据中通过用户自定义类型T通过参数指针或引用使得共享数据超出保护的作用域。
*   atomic<T>编译器通常会使用一个内部锁保护，而如果用户自定义类型T通过参数指针或引用可能产生死锁。总之限制T可以更利于原子指令。
*   注意某些原子操作可能会失败，比如atomic<float>、atomic<double>在compare_exchange_strong() 时和expected相等但是内置的值表示形式不同于expected，
*   还是返回false，没有原子算术操作针对浮点数;同理一些用户自定义的类型T由于内存的不同表示形式导致memcmp失败，从而使得一些相等的值仍返回false。

template < class T > struct atomic {
    bool is_lock_free() const volatile;
    //判断atomic<T>中的T对象是否为lock free的，若是返回true。
    //lock free(锁无关)指多个线程并发访问T不会出现data race，任何线程在任何时刻都可以不受限制的访问T

    bool is_lock_free() const;
    atomic() = default;//默认构造函数，T未初始化，可能后面被atomic_init(atomic<T>* obj,T val )函数初始化
    constexpr atomic(T val);//T由val初始化
    atomic(const atomic &) = delete;//禁止拷贝

    atomic & operator=(const atomic &) = delete;
    //atomic对象间的相互赋值被禁止，
    //但是可以显示转换再赋值，如atomic<int> a=static_cast<int>(b)这里假设atomic<int> b
    atomic & operator=(const atomic &) volatile = delete;//atomic间不能赋值
    T operator=(T val) volatile;//可以通过T类型对atomic赋值，如：atomic<int> a;a=10;
    T operator=(T val);
    operator  T() const volatile;//读取被封装的T类型值，是个类型转换操作，默认内存序是memory_order_seq需要其它内存序则调用load
    operator  T() const;//如：atomic<int> a,a==0或者cout<<a<<endl都使用了类型转换函数
    //以下函数可以指定内存序memory_order
    T exchange(T val, memory_order = memory_order_seq_cst) volatile;//将T的值置为val，并返回原来T的值
    T exchange(T val, memory_order = memory_order_seq_cst);
    void store(T val, memory_order = memory_order_seq_cst) volatile;//将T值设为val
    void store(T val, memory_order = memory_order_seq_cst);
    T load(memory_order = memory_order_seq_cst) const volatile;//访问T值
    T load(memory_order = memory_order_seq_cst) const;
    bool compare_exchange_weak(T& expected, T val, memory_order = memory_order_seq_cst) volatile;//该函数直接比较原子对象所封装的值与参数 expected 的物理内容，所以某些情况下，对象的比较操作在使用 operator==() 判断时相等，但 compare_exchange_weak 判断时却可能失败，因为对象底层的物理内容中可能存在位对齐或其他逻辑表示相同但是物理表示不同的值(比如 true 和 2 或 3，它们在逻辑上都表示"真"，但在物理上两者的表示并不相同)。可以虚假的返回false(和expected相同)。若本atomic的T值和expected相同则用val值替换本atomic的T值，返回true;若不同则用本atomic的T值替换expected，返回false。
    bool compare_exchange_weak(T &, T, memory_order = memory_order_seq_cst);
    bool compare_exchange_strong(T &, T, memory_order = memory_order_seq_cst) volatile;//
与compare_exchange_weak 不同, strong版本的 compare-and-exchange 操作不允许(spuriously 地)返回 false，即原子对象所封装的值与参数 expected 的物理内容相同，比较操作一定会为 true。不过在某些平台下，如果算法本身需要循环操作来做检查， compare_exchange_weak 的性能会更好。因此对于某些不需要采用循环操作的算法而言, 通常采用compare_exchange_strong 更好
    bool compare_exchange_strong(T &, T, memory_order = memory_order_seq_cst);
};
*/