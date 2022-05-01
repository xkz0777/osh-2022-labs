#include "shell.h"

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    Signal(SIGINT, sigint_handler); // 处理 ctrl + c
    // 用来存储读入的一行命令
    std::string cmd;

    // 读取 history，存为 vector<string>
    std::vector<std::string> all_history = read_history();

    while (true) {
        // 打印提示符
        print_prompt();

        // 读入一行。std::getline 结果不包含换行符。
        if (!std::getline(std::cin, cmd)) { // 处理 ctrl + d
            std::cout << "exit" << "\n";
            return 0;
        }
        // std::getline(std::cin, cmd);

        if (cmd.empty()) {
            continue;
        }
        std::ofstream history;
        std::string history_path = getenv("HOME") + std::string("/.shell_history");
        history.open(history_path.c_str(), std::ios_base::app);
        if (cmd[0] == '!') {
            if (cmd[1] == '!') { // 不会记录到 history，只会执行
                cmd = all_history[all_history.size() - 1];
                std::cout << cmd << "\n";
                std::cout.flush();
            } else { // 会记录到 history
                std::stringstream code_stream(cmd.substr(1));

                size_t code = 0;
                code_stream >> code;

                // 转换失败
                if (!code_stream.eof() || code_stream.fail()) {
                    std::cout << "Invalid number\n";
                    continue;
                } else if (code > all_history.size()) {
                    std::cout << "Invalid number\n";
                    continue;
                }
                cmd = all_history[code - 1];
                std::cout << cmd << "\n";
                std::cout.flush();
                history << cmd << "\n";
                all_history.push_back(cmd);
            }
        } else {
            history << cmd << "\n";
            all_history.push_back(cmd);
        }

        history.close();

        exec_pipe(cmd, all_history);
    }
}

// 执行内建命令
int exec_builtin(std::vector<std::string> &args, std::vector<std::string> &all_history) {
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
            exit(0);
        }

        // std::string 转 int
        std::stringstream code_stream(args[1]);
        int code = 0;
        code_stream >> code;

        // 转换失败
        if (!code_stream.eof() || code_stream.fail()) {
            std::cout << "Invalid exit code\n";
            exit(255);
        }

        exit(code);
    }

    // history
    else if (args[0] == "history") {
        int len = all_history.size();
        int width = lg(len);
        if (args.size() <= 1) {
            for (int i = 0; i < len; ++i) {
                std::cout << std::setw(width) << i + 1 << "  " << all_history[i] << "\n";
            }
        } else {
            std::stringstream code_stream(args[1]);
            int code = 0;
            code_stream >> code;

            // 转换失败
            if (!code_stream.eof() || code_stream.fail()) {
                std::cout << "Invalid number\n";
                exit(255);
            }

            for (int i = len - code; i < len; ++i) {
                std::cout << std::setw(width) << i + 1 << "  " << all_history[i] << "\n";
            }
        }
        return 0;
    }

    return -1; // 是外部命令
}

// 外部命令, 创建子进程完成
int exec_outer(std::vector<std::string> &args, int fd[]) {
    pid_t pid = Fork();

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
        dup2(fd[READ_END], STDIN_FILENO);
        dup2(fd[WRITE_END], STDOUT_FILENO);
        execvp(args[0].c_str(), arg_ptrs);

        // 所以这里直接报错
        exit(255);
    }

    // 这里只有父进程（原进程）才会进入
    int ret = wait(NULL);
    if (ret < 0) {
        std::cout << "wait failed";
    }
    return 0;
}

int *redir_process(std::vector<std::string> &args) {
    int len = args.size();
    int pos = 0;
    int *fd;
    fd = new int[2];
    fd[READ_END] = STDIN_FILENO;
    fd[WRITE_END] = STDOUT_FILENO;
    while (pos < len) {
        int fildes;
        if (args[pos] == "<") { // read
            args.erase(args.begin() + pos);
            len = args.size();
            fildes = open(args[pos].c_str(), O_RDONLY);
            fd[READ_END] = fildes;
        } else if (args[pos] == ">") { // write
            args.erase(args.begin() + pos);
            len = args.size();
            fildes = open(args[pos].c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            fd[WRITE_END] = fildes;
        } else if (args[pos] == ">>") { // append
            args.erase(args.begin() + pos);
            len = args.size();
            fildes = open(args[pos].c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
            fd[WRITE_END] = fildes;
        }
        pos++;
    }
    return fd;
}

// 执行单条命令，在子进程下 execute 时 terminate 为 true，运行完后终止进程
// 父进程下 terminate 为 false 
void execute(std::vector<std::string> &args, std::vector<std::string> &all_history, bool terminate) {
    replace_path(args);
    int res = exec_builtin(args, all_history);
    if (res == -1) {
        int *fd = redir_process(args);
        res = exec_outer(args, fd);
    }
    if (terminate) {
        exit(res);
    }
}

void sigint_handler(int) {
    std::cout << "\n";
    print_prompt();
    std::cout.flush();
}

std::vector<std::string> read_history() {
    std::vector<std::string> all_history;
    std::ifstream history;
    std::string item;
    std::string history_path = getenv("HOME") + std::string("/.shell_history");
    history.open(history_path.c_str(), std::ios_base::in);
    while (std::getline(history, item)) {
        all_history.push_back(item);
    }

    history.close();
    return all_history;
}

void exec_pipe(std::string &cmd, std::vector<std::string> &all_history) {
    // 处理管道
    std::vector<std::string> pipe_args = split(cmd, "|");

    // 没有可处理的命令
    if (pipe_args.empty()) {
        return;
    }

    int pipe_num = pipe_args.size(); // "|" 的个数
    if (pipe_num == 1) { // 没有管道
        // 按空格分割命令为单词
        std::vector<std::string> args = parse_cmd(pipe_args[0]);
        execute(args, all_history, false);
    }

    else if (pipe_num == 2) { // 两个进程之间通信
        int fd[2];
        Pipe(fd);

        pid_t pid = Fork(); // 创建第一个子进程

        if (pid == 0) { // 子进程
            close(fd[READ_END]);
            dup2(fd[WRITE_END], STDOUT_FILENO); // 将标准输出重定向到管道的写端口
            close(fd[WRITE_END]);

            std::vector<std::string> args = parse_cmd(pipe_args[0]);
            execute(args, all_history, true);
            exit(255);
        }

        else { // 父进程
            pid = Fork(); // 创建第二个子进程

            if (pid == 0) { // 子进程
                close(fd[WRITE_END]);
                dup2(fd[READ_END], STDIN_FILENO); // 将标准输入重定向到管道的读端口
                close(fd[READ_END]);

                std::vector<std::string> args = parse_cmd(pipe_args[1]);
                execute(args, all_history, true);
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
            int fd[2]; // 注意需要创建 n - 1 个不同管道
            if (i < pipe_num - 1) { // 最后一条不创建管道（不需要再把输出传给别人）
                Pipe(fd);
            }

            pid_t pid = Fork();

            if (pid == 0) { // 子进程
                if (i > 0) {
                    dup2(last_read_end, STDIN_FILENO); // 后面的不从标准输入拿，找 last_read_end
                }
                if (i < pipe_num - 1) {
                    dup2(fd[WRITE_END], STDOUT_FILENO); // 前面的输出到当前管道写端口，以传给下一个
                }

                std::vector<std::string> args = parse_cmd(pipe_args[i]);
                execute(args, all_history, true);
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
