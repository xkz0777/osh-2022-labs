#define _POSIX_C_SOURCE 200112L

/* C standard library */
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* POSIX */
#include <unistd.h>
#include <sys/user.h>
#include <sys/wait.h>

/* Linux */
#include <syscall.h>
#include <sys/ptrace.h>

int main(int argc, char **argv) {
    if (argc <= 1)
        FATAL("too few arguments: %d", argc);

    pid_t pid = fork();
    switch (pid) {
    case -1:
        exit(1);
    case 0:
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        execvp(argv[1], argv + 1);
        exit(1);
    }

    waitpid(pid, 0, 0);
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL);

    for (;;) {
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1)
            exit(1);
        if (waitpid(pid, 0, 0) == -1)
            exit(1);

        // 获取系统调用参数
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1)
            exit(1);
        long syscall = regs.orig_rax;
        fprintf(stderr, "%ld(%ld, %ld, %ld, %ld, %ld, %ld)\n",
            syscall,
            (long) regs.rdi, (long) regs.rsi, (long) regs.rdx,
            (long) regs.r10, (long) regs.r8, (long) regs.r9);

        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1)
            exit(1);
        if (waitpid(pid, 0, 0) == -1)
            exit(1);

        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
            if (errno == ESRCH)
                exit(regs.rdi);
            exit(1);
        }
    }
}
