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
// for trim
#include <algorithm> 

#define WRITE_END 1 // pipe 写端口
#define READ_END 0  // pipe 读端口
#define MAX_PIPE_NUM 20

std::vector<std::string> split(std::string s, const std::string &delimiter);

int exec_builtin(std::vector<std::string> &args, int fd[]);
int exec_outer(std::vector<std::string> &args, int fd[]);
int execute(std::vector<std::string> &args, bool terminate = true);
int redir_process(std::vector<std::string> &args, int fd[]);

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

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
        if (pipe_num == 1) { // 没有管道
            // 按空格分割命令为单词
            std::vector<std::string> args = split(pipe_args[0], " ");
            int res = execute(args, false);
            if (res) {
                return res;
            }
            continue;
        }

        else if (pipe_num == 2) { // 两个进程之间通信
            trim(pipe_args[0]);
            trim(pipe_args[1]);
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
                close(fd[READ_END]);
                dup2(fd[WRITE_END], STDOUT_FILENO); // 将标准输出重定向到管道的写端口
                close(fd[WRITE_END]);

                std::vector<std::string> args = split(pipe_args[0], " ");
                execute(args, true);
                exit(255);
            }

            else { // 父进程
                pid = fork(); // 创建第二个子进程

                if (pid < 0) {
                    std::cout << "Failed to create new process\n";
                    continue;
                }

                else if (pid == 0) { // 子进程
                    close(fd[WRITE_END]);
                    dup2(fd[READ_END], STDIN_FILENO); // 将标准输入重定向到管道的读端口
                    close(fd[READ_END]);

                    std::vector<std::string> args = split(pipe_args[1], " ");
                    execute(args, true);
                    exit(255);
                }

                else {
                    close(fd[WRITE_END]);
                    close(fd[READ_END]);

                    while (wait(NULL) > 0); // 等待所有子进程结束
                }
            }
        }

        else { // 多个进程
            int last_read_end = STDIN_FILENO; // 上个管道的读端，应该连到下个进程的写端
            for (int i = 0; i < pipe_num; ++i) {
                trim(pipe_args[i]);
                int fd[2]; // 注意需要创建 n - 1 个不同管道
                if (i < pipe_num - 1) { // 最后一条不创建管道（不需要再把输出传给别人）
                    int res = pipe(fd);
                    if (res < 0) {
                        std::cout << "Failed to create pipe\n";
                        continue;
                    }
                }

                int pid = fork();

                if (pid < 0) {
                    std::cout << "Failed to create new process\n";
                    continue;
                }

                else if (pid == 0) { // 子进程
                    if (i > 0) {
                        dup2(last_read_end, STDIN_FILENO); // 后面的不从标准输入拿，找 last_read_end
                    }
                    if (i < pipe_num - 1) {
                        dup2(fd[WRITE_END], STDOUT_FILENO); // 前面的输出到当前管道写端口，以传给下一个
                    }

                    std::vector<std::string> args = split(pipe_args[i], " ");
                    execute(args, true);
                    exit(255);
                }

                else { // 父进程
                    close(fd[WRITE_END]); // 父进程用不到 write_end
                    if (i > 0) {
                        close(last_read_end); // 关闭当前命令用完的 last_read_end
                    }
                    last_read_end = fd[READ_END]; // 更新 last_read_end
                }
            }
            while (wait(NULL) > 0); // 等待所有子进程结束
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
            return 1; // 1 表示执行失败，下同
        }

        // 调用系统 API
        int ret = chdir(args[1].c_str());
        if (ret < 0) {
            std::cout << "cd failed\n";
            return -1;
        }
        return 0; // 0 表示执行成功，下同
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
            return 1;
        } else {
            std::cout << ret << "\n";
            return 0;
        }
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
                return 1;
            }
            return 0;
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


    return -1; // 是外部命令
}

// 外部命令, 创建子进程完成
int exec_outer(std::vector<std::string> &args) {
    pid_t pid = fork();

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (size_t i = 0; i < args.size(); i++) {
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
    return 0;
}

// 执行单条命令，在子进程下 execute 时 terminate 为 true，正常运行完后终止进程（否则返回非 0 值）
// 父进程下 terminate 为 false 
int execute(std::vector<std::string> &args, bool terminate) {
    int res = exec_builtin(args);
    if (res == -1) {
        res = exec_outer(args);
    }
    if (terminate && res == 0) {
        exit(0);
    } else {
        return res;
    }
}
