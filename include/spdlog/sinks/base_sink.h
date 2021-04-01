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
    explicit base_sink(std::unique_ptr<spdlog::formatter> formatter);  // explicit ���캯�����࣬���ܷ�����Ӧ����ʽ����ת����ֻ������ʾ�ķ�ʽ��������ת��
    ~base_sink() override = default;                                   // override ���Ա�����������������д�麯���Ĵ���

    base_sink(const base_sink &) = delete;                             // delete ������Ҫ ����ֹʹ�á� �������Ա����������� ��= delete��
    base_sink(base_sink &&) = delete;                                  // ������ֵ�Ĺ��캯�� �ƶ����캯��

    base_sink &operator=(const base_sink &) = delete;                  // ��ֵ���캯��
    base_sink &operator=(base_sink &&) = delete;

    void log(const details::log_msg &msg) final;                       // final ������д
    void flush() final;
    void set_pattern(const std::string &pattern) final;                // �൱��ģ�巽�� ���� �����Ժ����ظ�
    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) final;

protected:
    // sink formatter
    std::unique_ptr<spdlog::formatter> formatter_;
    Mutex mutex_;

    virtual void sink_it_(const details::log_msg &msg) = 0;
    virtual void flush_() = 0;
    virtual void set_pattern_(const std::string &pattern);
    virtual void set_formatter_(std::unique_ptr<spdlog::formatter> sink_formatter);
};
} // namespace sinks
} // namespace spdlog

// ģ�����������ʵ��Ҫ����һ��
#ifdef SPDLOG_HEADER_ONLY
#include "base_sink-inl.h"
#endif
