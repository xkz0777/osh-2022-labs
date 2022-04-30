#include "utils.h"
// trim from start (in place)

inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

void print_prompt() {
    char hostname[PATH_MAX];
    gethostname(hostname, PATH_MAX - 1);
    char cwd[PATH_MAX];

    getcwd(cwd, PATH_MAX - 1);
    std::string cwd_string = cwd;

    char *pos = strstr(cwd, getenv("HOME"));
    if (pos != nullptr) {
        cwd_string = "~" + cwd_string.substr(strlen(getenv("HOME")));
    }
    std::cout << "\033[1;32m" << get_user_name() << "@" << hostname << "\033[0m" << ":" << "\033[1;34m" << cwd_string << "\033[0m" << "$ ";
}

int lg(int a) {
    int res = 0;
    while (a) {
        res++;
        a /= 10;
    }
    return res;
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        trim(token);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    trim(s);
    res.push_back(s);
    return res;
}

// 封装函数，下同
pid_t Fork() {
    pid_t pid = fork();
    if (pid < 0) {
        std::cout << "Failed to create new process\n";
        exit(255);
    }
    return pid;
}

void Pipe(int fd[]) {
    int res = pipe(fd);
    if (res < 0) {
        std::cout << "Failed to create pipe";
        exit(255);
    }
}

handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0) {
        std::cout << "Signal error\n";
        exit(255);
    }
    return (old_action.sa_handler);
}

void replace_path(std::vector<std::string> &args) { // 把路径中 ~ 换成家目录
    int len = args.size();
    for (int i = 0; i != len; ++i) {
        if (args[i][0] == '~') {
            args[i] = getenv("HOME") + args[i].substr(1);
        }
    }
}

inline std::string get_user_name() { // 获取用户名 https://www.codetd.com/article/12336470
    struct passwd *pwd;
    uid_t userid;
    userid = getuid();
    pwd = getpwuid(userid);
    return pwd->pw_name;
}

void add_space_str(std::string &arg) {
    size_t pos, pos1, pos2, pos3;
    pos1 = arg.find("<", 0);
    pos2 = arg.find(">", 0);
    pos3 = arg.find(">>", 0);
    while (pos1 != std::string::npos || pos2 != std::string::npos || pos3 != std::string::npos) {
        pos = std::min(pos1, std::min(pos2, pos3));
        if (pos == pos3) {
            arg = arg.substr(0, pos) + " " + arg.substr(pos, 2) + " " + arg.substr(pos + 2);
            pos1 = arg.find("<", pos + 3);
            pos2 = arg.find(">", pos + 3);
            pos3 = arg.find(">>", pos + 3);
        } else {
            arg = arg.substr(0, pos) + " " + arg[pos] + " " + arg.substr(pos + 1);
            pos1 = arg.find("<", pos + 2);
            pos2 = arg.find(">", pos + 2);
            pos3 = arg.find(">>", pos + 2);
        }
    }
}

// void add_space_vec(std::vector<std::string> &args) {
//     for (auto arg : args) {
//         add_space_str(arg);
//     }
// }

std::vector<std::string> parse_cmd(std::string &cmd) {
    add_space_str(cmd);
    std::vector<std::string> args = split(cmd, " ");
    return args;
}

// std::vector<std::string> parse_cmd(std::vector<std::string> &args) {
//     add_space_vec(args);
//     std::vector<std::string> new_vec;
//     for (auto arg : args) {
//         std::vector<std::string> split_arg = split(arg, " ");
//         new_vec.insert(new_vec.end(), split_arg.begin(), split_arg.end());
//     }
//     return new_vec;
// }