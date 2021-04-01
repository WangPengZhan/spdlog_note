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
*   �����������Ҫ��Ա������lock()��unlock()��
*   �ڽ����ٽ���ʱ��ִ��lock()���������������ʱ�Ѿ��������߳���ס����ǰ�߳��ڴ��Ŷӵȴ���
*   �˳��ٽ���ʱ��ִ��unlock()����������
*   (2)
*   ���õİ취�ǲ��á���Դ����ʱ��ʼ����(RAII)����������������������������ٽ�������Ϊ�׳��쳣��return�Ȳ�������û�н������˳������⡣
*   C++11�ı�׼�����ṩ��std::lock_guard��ģ����mutex��RAII��
*   lock_gurd�Ĺ����ǽӹ�mutex��lock��unlock�Ĺ���Ȩ
*   (3)
*   std::lock_guard��Ĺ��캯�����ÿ������죬�ҽ����ƶ����졣
*   std::lock_guard����˹��캯��������������û��������Ա������
*   ��std::lock_guard������ʱ�������mutex����(�����������mutex����)�ᱻ��ǰ�߳���ס��
*   ��lock_guard��������ʱ�����������mutex������Զ�������
*   ����Ҫ����Ա�ֶ�����lock��unlock��mutex���������ͽ���������
*   lock_guard���󲢲��������mutex������������ڣ�lock_guard����ֻ�Ǽ���mutex����������ͽ��������������̶߳Ի�����������
*   ����ĳ��lock_guard��������������ڣ�����������������һֱ��������״̬��
*   ��lock_guard���������ڽ���֮�����������������ᱻ������
*   ����Ա���Էǳ������ʹ��lock_guard�������õ����쳣��ȫ���⡣
*   std::lock_guard�ڹ���ʱֻ������һ�Σ�����������ʱ������
*
* 2.unique_lock��
*��1��
*  ���ṩ��lock()��unlock()�ӿڣ��ܼ�¼���ڴ�����������û����״̬��
*  �ڹ��캯���ﳢ�Լ�����������Ѿ���������������
*  ������������ʱ�򣬻���ݵ�ǰ״̬�������Ƿ�Ҫ���н�����lock_guard��һ�����������
*��2��
*  std::unique_lock��Ĺ��캯�����ÿ������죬�������ƶ����졣
*
* lock_gurd�ĵڶ�����
* eg��std::lock_guard<std::mutex> sbguard1(my_mutex1, std::adopt_lock);
*  (1)  std::adopt_lock
        ���ã���ʾ����������Ѿ���lock�ˣ������Ҫ�ѻ�������ǰlock�� �����߻ᱨ�쳣����
        ԭ��std::adopt_lock��ǵ�Ч�����Ǽ������һ���Ѿ�ӵ���˻�����������Ȩ���Ѿ�lock�ɹ��ˣ���
            ֪ͨlock_guard����Ҫ�ٹ��캯����lock����������ˡ�


* ��2�� std::try_to_lock
        ���ã�������mutex��lock()ȥ�������mutex,�����û�������ɹ�����Ҳ���������أ����������������

* ��3�� std::defer_lock
*
*/

#ifndef __MINGW32__
    // try to enqueue and block if no room left
    void enqueue(T &&item)
    {
        // 1. û���õ���
        //      �ȴ����ͷ�
        // 2. �õ���
        //     wait
        //    (1) ��������Ƿ����
        //       ������������
        //       ���������ͷ������߳�˯�� �ȴ�����
        //    (2) ����
        //        �߳�����ִ���������� ��������
        //        �ȴ��߳� �������»���� ִ�н�����������
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            pop_cv_.wait(lock, [this] { return !this->q_.full(); });
            q_.push_back(std::move(item));
        }

        // condition_variable ���� wait �� wait_for �� wait_until �� notify_one �� notify_all ��Ա������ͬʱ���á�
        // ֪ͨ׼���õ����� ����ҪΪ֪ͨ����
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
