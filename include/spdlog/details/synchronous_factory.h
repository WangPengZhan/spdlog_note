// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include "registry.h"

namespace spdlog {

// Default logger factory-  creates synchronous loggers
class logger;

/*
*   省略号的作用有两个：
*   1.声明一个参数包T... args，这个参数包中可以包含0到任意个模板参数；
*   2.在模板定义的右边，可以将参数包展开成一个一个独立的参数。
*   展开可变模版参数函数的方法一般有两种：
*   1.通过递归函数来展开参数包。 终止条件：空参数，一个参数都可以
*   2.通过逗号表达式来展开参数包。
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