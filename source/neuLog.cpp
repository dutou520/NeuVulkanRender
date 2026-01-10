#include "neuLog.h"
#include "spdlog/sinks/stdout_color_sinks.h" //控制台输出带颜色
#include <spdlog/async.h>

namespace neurender {
// 定义一个指向spdlog::logger的共享指针，初始值为空
std::shared_ptr<spdlog::logger> NeuLog::NeuLoggerInstance{};
void NeuLog::Init() {
  // 1. 对于异步日志，必须初始化线程池（队列大小，线程数）
  //    这里设置 8192 个槽位和 1 个后台线程
  spdlog::init_thread_pool(8192, 1);

  // 2. 使用 async_factory 创建异步日志器
  NeuLoggerInstance =
      spdlog::stdout_color_mt<spdlog::async_factory>("Async_neuLogger");

  NeuLoggerInstance->set_level(spdlog::level::trace);
  NeuLoggerInstance->set_pattern(
      "%^%H:%M:%S:%e [%P-%t] [%1!L] [%20s:%-4#] - %v%$");

  // 3. 设置遇到报错时立即强制冲刷缓冲区
  NeuLoggerInstance->flush_on(spdlog::level::err);

  // 4. 设置为全局默认日志器
  spdlog::set_default_logger(NeuLoggerInstance);
}
} // namespace neurender