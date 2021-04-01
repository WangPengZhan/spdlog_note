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
*   atomic<T> ģ����
*   ����T��trivially copyable type���㣺
*   1.Ҫôȫ�������˿���/�ƶ�/��ֵ������Ҫôȫ��û����;
*   2.û�����Ա;
*   3.����������κη�static��Ա����trivally copyable
*   ���͵���������bool��int������trivally copyable��
*   ����class triviall{public : int x};Ҳ�ǡ�
*   T�ܹ���memcpy��memcmp����ʹ�ã��Ӷ�֧��compare/exchangeϵ�к�����
*   ��һ�����򣺲�Ҫ�ڱ���������ͨ���û��Զ�������Tͨ������ָ�������ʹ�ù������ݳ���������������
*   atomic<T>������ͨ����ʹ��һ���ڲ���������������û��Զ�������Tͨ������ָ������ÿ��ܲ�����������֮����T���Ը�����ԭ��ָ�
*   ע��ĳЩԭ�Ӳ������ܻ�ʧ�ܣ�����atomic<float>��atomic<double>��compare_exchange_strong() ʱ��expected��ȵ������õ�ֵ��ʾ��ʽ��ͬ��expected��
*   ���Ƿ���false��û��ԭ������������Ը�����;ͬ��һЩ�û��Զ��������T�����ڴ�Ĳ�ͬ��ʾ��ʽ����memcmpʧ�ܣ��Ӷ�ʹ��һЩ��ȵ�ֵ�Է���false��

template < class T > struct atomic {
    bool is_lock_free() const volatile;
    //�ж�atomic<T>�е�T�����Ƿ�Ϊlock free�ģ����Ƿ���true��
    //lock free(���޹�)ָ����̲߳�������T�������data race���κ��߳����κ�ʱ�̶����Բ������Ƶķ���T

    bool is_lock_free() const;
    atomic() = default;//Ĭ�Ϲ��캯����Tδ��ʼ�������ܺ��汻atomic_init(atomic<T>* obj,T val )������ʼ��
    constexpr atomic(T val);//T��val��ʼ��
    atomic(const atomic &) = delete;//��ֹ����

    atomic & operator=(const atomic &) = delete;
    //atomic�������໥��ֵ����ֹ��
    //���ǿ�����ʾת���ٸ�ֵ����atomic<int> a=static_cast<int>(b)�������atomic<int> b
    atomic & operator=(const atomic &) volatile = delete;//atomic�䲻�ܸ�ֵ
    T operator=(T val) volatile;//����ͨ��T���Ͷ�atomic��ֵ���磺atomic<int> a;a=10;
    T operator=(T val);
    operator  T() const volatile;//��ȡ����װ��T����ֵ���Ǹ�����ת��������Ĭ���ڴ�����memory_order_seq��Ҫ�����ڴ��������load
    operator  T() const;//�磺atomic<int> a,a==0����cout<<a<<endl��ʹ��������ת������
    //���º�������ָ���ڴ���memory_order
    T exchange(T val, memory_order = memory_order_seq_cst) volatile;//��T��ֵ��Ϊval��������ԭ��T��ֵ
    T exchange(T val, memory_order = memory_order_seq_cst);
    void store(T val, memory_order = memory_order_seq_cst) volatile;//��Tֵ��Ϊval
    void store(T val, memory_order = memory_order_seq_cst);
    T load(memory_order = memory_order_seq_cst) const volatile;//����Tֵ
    T load(memory_order = memory_order_seq_cst) const;
    bool compare_exchange_weak(T& expected, T val, memory_order = memory_order_seq_cst) volatile;//�ú���ֱ�ӱȽ�ԭ�Ӷ�������װ��ֵ����� expected ���������ݣ�����ĳЩ����£�����ıȽϲ�����ʹ�� operator==() �ж�ʱ��ȣ��� compare_exchange_weak �ж�ʱȴ����ʧ�ܣ���Ϊ����ײ�����������п��ܴ���λ����������߼���ʾ��ͬ���������ʾ��ͬ��ֵ(���� true �� 2 �� 3���������߼��϶���ʾ"��"���������������ߵı�ʾ������ͬ)��������ٵķ���false(��expected��ͬ)������atomic��Tֵ��expected��ͬ����valֵ�滻��atomic��Tֵ������true;����ͬ���ñ�atomic��Tֵ�滻expected������false��
    bool compare_exchange_weak(T &, T, memory_order = memory_order_seq_cst);
    bool compare_exchange_strong(T &, T, memory_order = memory_order_seq_cst) volatile;//
��compare_exchange_weak ��ͬ, strong�汾�� compare-and-exchange ����������(spuriously ��)���� false����ԭ�Ӷ�������װ��ֵ����� expected ������������ͬ���Ƚϲ���һ����Ϊ true��������ĳЩƽ̨�£�����㷨������Ҫѭ������������飬 compare_exchange_weak �����ܻ���á���˶���ĳЩ����Ҫ����ѭ���������㷨����, ͨ������compare_exchange_strong ����
    bool compare_exchange_strong(T &, T, memory_order = memory_order_seq_cst);
};
*/