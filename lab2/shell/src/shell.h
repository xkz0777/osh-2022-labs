#pragma once
#include "utils.h"

#define WRITE_END 1 // pipe 写端口
#define READ_END 0  // pipe 读端口
#define MAX_PIPE_NUM 20

int exec_builtin(std::vector<std::string> &args, std::vector<std::string> &all_history);
int exec_outer(std::vector<std::string> &args, int fd[]);
void execute(std::vector<std::string> &args, std::vector<std::string> &all_history, bool terminate);
void exec_pipe(std::string &, std::vector<std::string> &);
int *redir_process(std::vector<std::string> &args);
void sigint_handler(int);
std::vector<std::string> read_history();

std::unordered_map<std::string, std::string> alias_table;
