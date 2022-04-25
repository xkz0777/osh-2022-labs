// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>

#define WRITE_END 1 // pipe 写端口
#define READ_END 0  // pipe 读端口

std::vector<std::string> split(std::string s, const std::string &delimiter);

int exec_builtin(std::vector<std::string> &args);
int exec_outer(std::vector<std::string> &args);
int execute(std::vector<std::string> &args);

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // 用来存储读入的一行命令
    std::string cmd;
    while (true) {
      // 打印提示符
        std::cout << "$ ";

        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);

        // 处理管道
        std::vector<std::string> pipe_args = split(cmd, "|");

        // 没有可处理的命令
        if (pipe_args.empty()) {
            continue;
        }

        int pipe_num = pipe_args.size(); // "|" 的个数
        if (pipe_num == 0) { // 没有管道
            // 按空格分割命令为单词
            std::vector<std::string> args = split(pipe_args[0], " ");
            int return_code = execute(args);
            if (return_code == 1) {
                continue;
            } else {
                return return_code;
            }
        } else if (pipe_num == 1) { // 两个进程之间通信
            int fd[2];
            int res = pipe(fd);
            if (res < 0) {
                std::cout << "Failed to create pipe\n";
                continue;
            }

            int pid = fork(); // 创建第一个子进程

            if (pid < 0) {
                std::cout << "Failed to create new process\n";
                continue;
            }

            else if (pid == 0) { // 子进程
                close(fd[WRITE_END]);
                dup2(fd[READ_END], STDOUT_FILENO); // 将标准输出重定向到管道的读端口
                close(fd[READ_END]);

                std::vector<std::string> args = split(pipe_args[0], " ");
                int return_code = execute(args);
                if (return_code == 1) {
                    continue;
                } else {
                    return return_code;
                }
            }

            else { // 父进程
                pid = fork(); // 创建第二个子进程

                if (pid < 0) {
                    std::cout << "Failed to create new process\n";
                    continue;
                }

                else if (pid == 0) { // 子进程
                    close(fd[READ_END]);
                    dup2(fd[WRITE_END], STDIN_FILENO); // 将标准输入重定向到管道的写端口
                    close(fd[WRITE_END]);

                    std::vector<std::string> args = split(pipe_args[1], " ");
                    int return_code = execute(args);
                    if (return_code == 1) {
                        continue;
                    } else {
                        return return_code;
                    }
                }

                else {
                    close(fd[WRITE_END]);
                    close(fd[READ_END]);

                    while (wait(NULL) > 0);
                }
            }
        }
    }
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    res.push_back(s);
    return res;
}

// 执行内建命令
int exec_builtin(std::vector<std::string> &args) {
    // 更改工作目录为目标目录
    if (args[0] == "cd") {
        if (args.size() <= 1) {
            // 输出的信息尽量为英文，非英文输出（其实是非 ASCII 输出）在没有特别配置的情况下（特别是 Windows 下）会乱码
            // 如感兴趣可以自行搜索 GBK Unicode UTF-8 Codepage UTF-16 等进行学习
            std::cout << "Insufficient arguments\n";
            // 不要用 std::endl，std::endl = "\n" + fflush(stdout)
            return 1; // 1 表示继续
        }

        // 调用系统 API
        int ret = chdir(args[1].c_str());
        if (ret < 0) {
            std::cout << "cd failed\n";
        }
        return 1;
    }

    // 显示当前工作目录
    else if (args[0] == "pwd") {
        std::string cwd;

        // 预先分配好空间
        cwd.resize(PATH_MAX);

        // std::string to char *: &s[0]（C++17 以上可以用 s.data()）
        // std::string 保证其内存是连续的
        const char *ret = getcwd(&cwd[0], PATH_MAX);
        if (ret == nullptr) {
            std::cout << "cwd failed\n";
        } else {
            std::cout << ret << "\n";
        }
        return 1;
    }

    // 设置环境变量
    else if (args[0] == "export") {
        for (auto i = ++args.begin(); i != args.end(); i++) {
            std::string key = *i;

            // std::string 默认为空
            std::string value;

            // std::string::npos = std::string end
            // std::string 不是 nullptr 结尾的，但确实会有一个结尾字符 npos
            size_t pos;
            if ((pos = i->find('=')) != std::string::npos) {
                key = i->substr(0, pos);
                value = i->substr(pos + 1);
            }

            int ret = setenv(key.c_str(), value.c_str(), 1);
            if (ret < 0) {
                std::cout << "export failed\n";
            }
        }
    }

    // 退出
    else if (args[0] == "exit") {
        if (args.size() <= 1) {
            return 0;
        }

        // std::string 转 int
        std::stringstream code_stream(args[1]);
        int code = 0;
        code_stream >> code;

        // 转换失败
        if (!code_stream.eof() || code_stream.fail()) {
            std::cout << "Invalid exit code\n";
            return 1;
        }

        return code;
    }

    else {
        return -1;
    }
}

// 外部命令, 创建子进程完成
int exec_outer(std::vector<std::string> &args) {
    pid_t pid = fork();

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    if (pid == 0) {
        // 这里只有子进程才会进入
        // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
        // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
        execvp(args[0].c_str(), arg_ptrs);

        // 所以这里直接报错
        exit(255);
    }

    // 这里只有父进程（原进程）才会进入
    int ret = wait(nullptr);
    if (ret < 0) {
        std::cout << "wait failed";
    }
}

// 执行单条命令
int execute(std::vector<std::string> &args) {
    int res = exec_builtin(args);
    if (res == -1) {
        exec_outer(args);
        return 1;
    } else {
        return res;
    }
}
