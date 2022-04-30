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
int exec_builtin(std::vector<std::string> &args);
int exec_outer(std::vector<std::string> &args);
void execute(std::vector<std::string> &args, bool terminate);
int *redir_process(std::vector<std::string> &args);
pid_t Fork();
void Pipe(int fd[]);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
void sigint_handler(int signum);