#pragma once
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
// file stream
#include <fstream>
// control width
#include <iomanip>
// 获取用户名和主机名
#include <unistd.h>
#include <pwd.h>
#include <cstring>

pid_t Fork();
void Pipe(int fd[]);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
std::vector<std::string> split(std::string s, const std::string &delimiter);
int lg(int);
inline void ltrim(std::string &s);
inline void rtrim(std::string &s);
inline void trim(std::string &s);
void replace_path(std::vector<std::string> &args);
inline std::string get_user_name();
void print_prompt();
void add_space_str(std::string &); // 给命令中所有 < > 和 >> 添加空格
// void add_space_vec(std::vector<std::string> &);
std::vector<std::string> parse_cmd(std::string &cmd);
// std::vector<std::string> parse_cmd(std::vector<std::string> &args);
