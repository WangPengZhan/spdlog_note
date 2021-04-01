// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// multi producer-multi consumer blocking queue.
// enqueue(..) - will block until room found to put the new message.
// enqueue_nowait(..) - will return immediately with false if no room left in
// the queue.
// dequeue_for(..) - will block until the queue is not empty or timeout have
// passed.

#include <spdlog/details/circular_q.h>

#include <condition_variable>
#include <mutex>

namespace spdlog {
namespace details {

template<typename T>
class mpmc_blocking_queue
{
public:
    using item_type = T;
    explicit mpmc_blocking_queue(size_t max_items)
        : q_(max_items)
    {}

/*
*  1.lock_guard
*   (1)
*   互斥类的最重要成员函数是lock()和unlock()。
*   在进入临界区时，执行lock()加锁操作，如果这时已经被其它线程锁住，则当前线程在此排队等待。
*   退出临界区时，执行unlock()解锁操作。
*   (2)
*   更好的办法是采用”资源分配时初始化”(RAII)方法来加锁、解锁，这避免了在临界区中因为抛出异常或return等操作导致没有解锁就退出的问题。
*   C++11的标准库中提供了std::lock_guard类模板做mutex的RAII。
*   lock_gurd的功能是接管mutex的lock和unlock的管理权
*   (3)
*   std::lock_guard类的构造函数禁用拷贝构造，且禁用移动构造。
*   std::lock_guard类除了构造函数和析构函数外没有其它成员函数。
*   在std::lock_guard对象构造时，传入的mutex对象(即它所管理的mutex对象)会被当前线程锁住。
*   在lock_guard对象被析构时，它所管理的mutex对象会自动解锁。
*   不需要程序员手动调用lock和unlock对mutex进行上锁和解锁操作。
*   lock_guard对象并不负责管理mutex对象的生命周期，lock_guard对象只是简化了mutex对象的上锁和解锁操作，方便线程对互斥量上锁，
*   即在某个lock_guard对象的生命周期内，它所管理的锁对象会一直保持上锁状态；
*   而lock_guard的生命周期结束之后，它所管理的锁对象会被解锁。
*   程序员可以非常方便地使用lock_guard，而不用担心异常安全问题。
*   std::lock_guard在构造时只被锁定一次，并且在销毁时解锁。
*
* 2.unique_lock。
*（1）
*  它提供了lock()和unlock()接口，能记录现在处于上锁还是没上锁状态。
*  在构造函数里尝试加锁，如果锁已经被加锁，则阻塞
*  但是在析构的时候，会根据当前状态来决定是否要进行解锁（lock_guard就一定会解锁）。
*（2）
*  std::unique_lock类的构造函数禁用拷贝构造，但可以移动构造。
*
* lock_gurd的第二参数
* eg：std::lock_guard<std::mutex> sbguard1(my_mutex1, std::adopt_lock);
*  (1)  std::adopt_lock
        作用：表示这个互斥量已经被lock了（你必须要把互斥量提前lock了 ，否者会报异常）；
        原理：std::adopt_lock标记的效果就是假设调用一方已经拥有了互斥量的所有权（已经lock成功了）；
            通知lock_guard不需要再构造函数中lock这个互斥量了。


* （2） std::try_to_lock
        作用：尝试用mutex的lock()去锁定这个mutex,但如果没有锁定成功，我也会立即返回，并不会阻塞在那里；

* （3） std::defer_lock
*
*/

#ifndef __MINGW32__
    // try to enqueue and block if no room left
    void enqueue(T &&item)
    {
        // 1. 没有拿到锁
        //      等待锁释放
        // 2. 拿到锁
        //     wait
        //    (1) 检查条件是否成立
        //       成立处理数据
        //       不成立，释放锁，线程睡眠 等待唤醒
        //    (2) 唤醒
        //        线程正在执行其他任务 不起作用
        //        等待线程 尝试重新获得锁 执行接下来的任务
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            pop_cv_.wait(lock, [this] { return !this->q_.full(); });
            q_.push_back(std::move(item));
        }

        // condition_variable 容许 wait 、 wait_for 、 wait_until 、 notify_one 及 notify_all 成员函数的同时调用。
        // 通知准备好的数据 不需要为通知上锁
        push_cv_.notify_one();
    }

    // enqueue immediately. overrun oldest message in the queue if no room left.
    void enqueue_nowait(T &&item)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            q_.push_back(std::move(item));
        }
        push_cv_.notify_one();
    }

    // try to dequeue item. if no item found. wait upto timeout and try again
    // Return true, if succeeded dequeue item, false otherwise
    bool dequeue_for(T &popped_item, std::chrono::milliseconds wait_duration)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!push_cv_.wait_for(lock, wait_duration, [this] { return !this->q_.empty(); }))
            {
                return false;
            }
            popped_item = std::move(q_.front());
            q_.pop_front();
        }
        pop_cv_.notify_one();
        return true;
    }

#else
    // apparently mingw deadlocks if the mutex is released before cv.notify_one(),
    // so release the mutex at the very end each function.

    // try to enqueue and block if no room left
    void enqueue(T &&item)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        pop_cv_.wait(lock, [this] { return !this->q_.full(); });
        q_.push_back(std::move(item));
        push_cv_.notify_one();
    }

    // enqueue immediately. overrun oldest message in the queue if no room left.
    void enqueue_nowait(T &&item)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        q_.push_back(std::move(item));
        push_cv_.notify_one();
    }

    // try to dequeue item. if no item found. wait upto timeout and try again
    // Return true, if succeeded dequeue item, false otherwise
    bool dequeue_for(T &popped_item, std::chrono::milliseconds wait_duration)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!push_cv_.wait_for(lock, wait_duration, [this] { return !this->q_.empty(); }))
        {
            return false;
        }
        popped_item = std::move(q_.front());
        q_.pop_front();
        pop_cv_.notify_one();
        return true;
    }

#endif

    size_t overrun_counter()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return q_.overrun_counter();
    }

    size_t size()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return q_.size();
    }

private:
    std::mutex queue_mutex_;
    std::condition_variable push_cv_;
    std::condition_variable pop_cv_;
    spdlog::details::circular_q<T> q_;
};
} // namespace details
} // namespace spdlog
