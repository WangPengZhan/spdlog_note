// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/base_sink.h>
#endif

#include <spdlog/common.h>
#include <spdlog/pattern_formatter.h>

#include <memory>

/*
*   1. unique_ptr
*   unique_ptr�Ǹ���ռָ�룬unique_ptr��ָ���ڴ�Ϊ�Լ����У�
*   ĳ��ʱ��ֻ����һ��unique_ptrָ��һ�������Ķ��󣬲�֧�ֿ����͸�ֵ��
*   ֧��std::move����ӵ��Ȩת��
*   �ͷŷ�ʽ:(1)reset() (2)reset(new objection) (3)ptr = nullptr (4) std::move(ptr)
*   releaseֻ�ͷŹ���Ȩ�������ͷ��ڴ�
*
*   2 shared_ptr
*   ʵ�ֵķ�ʽ: ���ü��� ָ��̬�����int
*   ���췽ʽ: make_shared<typename(arg...)>
*
*   3.weak_ptr
*   weak_ptr��Ϊ���shared_ptr�������һ������ָ����Э��shared_ptr������˫ӵ�бջ�
*   �����Դ�һ��shared_ptr����һ��weak_ptr�����죬���Ĺ�������������������ü��������ӻ���١�
*   û������ *�� -> ������ʹ��lock���һ�����õ�shared_ptr����
*   weak_ptr��ʹ�ø�Ϊ����һ�㣬������ָ��shared_ptrָ��ָ��Ķ����ڴ棬ȴ����ӵ�и��ڴ棬
*   ��ʹ��weak_ptr��Աlock����ɷ�����ָ���ڴ��һ��share_ptr����������ָ�����ڴ��Ѿ���Чʱ������ָ���ֵnullptr��
*   ע�⣺weak_ptr����ӵ����Դ������Ȩ�����Բ���ֱ��ʹ����Դ�� ���Դ�һ��weak_ptr����һ��shared_ptr��ȡ�ù�����Դ������Ȩ��
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
