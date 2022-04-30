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

pid_t Fork();
void Pipe(int fd[]);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
std::vector<std::string> split(std::string s, const std::string &delimiter);
int lg(int);
static inline void ltrim(std::string &s);
static inline void rtrim(std::string &s);
static inline void trim(std::string &s);
