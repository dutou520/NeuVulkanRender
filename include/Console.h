#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace neurender {

/**
 * @brief Console 命令行调试系统
 *
 * 在后台线程监听标准输入 (stdin)，命令进入线程安全队列后，
 * 由主循环在渲染线程上逐条执行，保证与渲染资源访问无竞争。
 *
 * 用法：
 *   1. 启动后直接在该进程的终端窗口输入命令
 *   2. 支持管道/重定向批量注入命令：
 *      echo "load project D:/xxx" | NeuVulkanRender.exe
 *
 * 命令格式：<command> [args...]，路径含空格可用双引号包裹。
 */
class Console {
public:
  Console() = delete;
  Console(const Console &) = delete;
  Console &operator=(const Console &) = delete;

  using CommandHandler = std::function<bool(const std::vector<std::string> &)>;

  /// @brief 启动 stdin 监听线程
  static void Init();

  /// @brief 停止监听线程（线程阻塞在 getline 时直接分离）
  static void Shutdown();

  /// @brief 主循环每帧调用，在渲染线程上执行队列中的命令
  static void Update();

  /// @brief 注册命令处理器
  static void RegisterCommand(const std::string &name,
                              const std::string &usage,
                              const std::string &description,
                              CommandHandler handler);

  /// @brief 手动注入一条命令（线程安全），供其他系统调用
  static void Execute(const std::string &commandLine);

  /// @brief 向控制台输出调试信息
  static void Print(const std::string &message);

  struct CommandEntry {
    std::string name;
    std::string usage;
    std::string description;
    CommandHandler handler;
  };

  // 已注册的命令表（help 命令需要遍历）
  static std::vector<CommandEntry> s_Commands;

private:
  static void InputThreadFunc();

  static std::vector<std::string> s_PendingCommands;
  static std::mutex s_QueueMutex;
  static std::thread s_InputThread;
  static bool s_StopRequested;
  static bool s_Initialized;
};

} // namespace neurender
