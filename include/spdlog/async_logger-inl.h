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
     *  weak_ptr�����Ϊ��shared_ptr��ͬ���������Դ�һ��shared_ptr������һ��weak_ptr�����죬�����Դ�Ĺ۲�Ȩ��
     *  ��weak_ptrû�й�����Դ�����Ĺ��첻������ָ�����ü��������ӡ�ͬ������weak_ptr����ʱҲ���ᵼ�����ü����ļ��٣���ֻ��һ�������ع۲��ߡ�
     *  weak_ptrû������operator*��->����������ģ���Ϊ��������ָ�룬���ܲ�����Դ������������ԭ��
     *  ��������ʹ��һ���ǳ���Ҫ�ĳ�Ա����lock()�ӱ��۲��shared_ptr���һ�����õ�shared_ptr���󣬴Ӷ�������Դ��
     *  weak_ptr���ڽ�������ü�����ģ��ѭ���������⣬weak_ptrָ��һ�����󣬲��������ö�������ü�������
     *  weak_ptr�������shared_ptrʹ�ã�����Ӱ�춯̬������������ڣ����������񲢲�Ӱ���������ü�������
     *  weak_ptr�ṩ��expired()��lock()��Ա������
     *  ǰ�������ж�weak_ptrָ��Ķ����Ƿ��ѱ����٣����߷�������ָ�����shared_ptr����ָ��(��������ʱ���ء��ա�shared_ptr)��
     */

    /*
    * shared_from_this()����;
    * enable_shared_from_this��һ��ģ���࣬������ͷ�ļ�<memory>����ԭ��Ϊ��
    * template< class T > class enable_shared_from_this;
    * std::enable_shared_from_this ����һ�����󣨼�������Ϊ t �����ѱ�һ�� std::shared_ptr ���� pt ����
    * ��ȫ��������������� std::shared_ptr ʵ����������Ϊ pt1, pt2, ... �� �������� pt ������� t ������Ȩ��
    * ��һ���� T �̳� std::enable_shared_from_this<T> �����Ϊ���� T �ṩ��Ա������ shared_from_this ��
    * �� T ���Ͷ��� t ��һ��Ϊ��Ϊ pt �� std::shared_ptr<T> ��������ʱ������ T::shared_from_this ��Ա������
    * ���᷵��һ���µ� std::shared_ptr<T> �������� pt ���� t ������Ȩ��
    * (1)ʹ��ԭ��
     1.�ѵ�ǰ�������Ϊ����������������ʱ��ΪʲôҪ����share_ptr�أ�ֱ�Ӵ���thisָ�벻������
         һ����ָ�봫�ݸ������ߣ�˭Ҳ��֪�������߻��ʲô�����������delete�˸ö��󣬶�share_tr��ʱ��ָ��ö���

     2.��������share_ptr������share_ptr <this>
         ���������2���ǹ����share_ptrָ��һ������������2�������ö���
    * (2)ʵ��ԭ��
     enable_shared_from_this��һ��ʵ�ַ����ǣ����ڲ���һ��weak_ptr���͵ĳ�Ա����_Wptr��
     ��shared_ptr�����ʱ�������ģ�����ͼ̳���enable_shared_from_this�����_Wptr���г�ʼ��������
     ������������shared_from_this������ʱ�򣬾��ܹ�ͨ��weak_ptr�������Ӧ��shared_ptr��
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
