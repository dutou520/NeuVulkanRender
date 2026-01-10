#pragma once

// 定义日志级别为TRACE
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "neulog_export.h"
#include "spdlog/spdlog.h" //日志库
namespace neurender {
// 日志类
class NEULOG_API NeuLog {
public:
  NeuLog() = delete;                          // 禁止默认构造函数
  NeuLog(const NeuLog &) = delete;            // 禁止拷贝构造函数
  NeuLog &operator=(const NeuLog &) = delete; // 禁止拷贝赋值运算符

  static void Init();
  // 获取日志实例
  static spdlog::logger *GetLoggerInstance() {
    // 断言sLoggerInstance不为空，如果为空，则输出错误信息
    //"Logger instance is null ,may not init()"
    assert(NeuLoggerInstance && "Logger instance is null ,may not init()");
    // 返回sLoggerInstance的指针
    return NeuLoggerInstance.get();
  };

private:
  static std::shared_ptr<spdlog::logger> NeuLoggerInstance;
};
// 定义日志宏
// 定义一个宏，用于记录跟踪级别的日志
#define LOG_T(...)                                                             \
  SPDLOG_LOGGER_TRACE(neurender::NeuLog::GetLoggerInstance(), ##__VA_ARGS__)
// 定义一个宏，用于记录调试级别的日志
#define LOG_D(...)                                                             \
  SPDLOG_LOGGER_DEBUG(neurender::NeuLog::GetLoggerInstance(), ##__VA_ARGS__)
// 定义一个宏，用于记录信息级别的日志
#define LOG_I(...)                                                             \
  SPDLOG_LOGGER_INFO(neurender::NeuLog::GetLoggerInstance(), ##__VA_ARGS__)
// 定义一个宏，用于记录警告级别的日志
#define LOG_W(...)                                                             \
  SPDLOG_LOGGER_WARN(neurender::NeuLog::GetLoggerInstance(), ##__VA_ARGS__)
// 定义一个宏，用于记录错误级别的日志
#define LOG_E(...)                                                             \
  SPDLOG_LOGGER_ERROR(neurender::NeuLog::GetLoggerInstance(), ##__VA_ARGS__)
//__VA_ARGS__是参数列表，##用于删除参数列表中的逗号
} // namespace neurender
