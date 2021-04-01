// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include "registry.h"

namespace spdlog {

// Default logger factory-  creates synchronous loggers
class logger;

/*
*   ʡ�Ժŵ�������������
*   1.����һ��������T... args������������п��԰���0�������ģ�������
*   2.��ģ�嶨����ұߣ����Խ�������չ����һ��һ�������Ĳ�����
*   չ���ɱ�ģ����������ķ���һ�������֣�
*   1.ͨ���ݹ麯����չ���������� ��ֹ�������ղ�����һ������������
*   2.ͨ�����ű��ʽ��չ����������
*   https://www.cnblogs.com/qicosmos/p/4325949.html
*   auto lambda
*/


struct synchronous_factory
{
    template<typename Sink, typename... SinkArgs>
    static std::shared_ptr<spdlog::logger> create(std::string logger_name, SinkArgs &&...args)
    {
        auto sink = std::make_shared<Sink>(std::forward<SinkArgs>(args)...);
        auto new_logger = std::make_shared<spdlog::logger>(std::move(logger_name), std::move(sink));
        details::registry::instance().initialize_logger(new_logger);
        return new_logger;
    }
};
} // namespace spdlog