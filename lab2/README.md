# Lab2 Report

## Shell

### Compile

`make` 后可执行文件路径：`lab2/shell/bin/shell`

### 命令提示符

提示符格式与本机的 Bash 相同，为 `<username>@<hostname>:$<cwd>` 的格式，`<username>@<hostname>` 部分是粗体的绿色，`cwd` 部分为蓝色。同时为了尽量与 Bash 一致，对路径中的 `$HOME` 部分做了替换，换成了 `~`。

### 命令解析

支持在路径中输入 `~`，例如 `cat ~/.bash_history`。

支持引号和转义的空格，例如 `echo "hello world"` 和 `echo hello\ world`。

支持转义符（仅对 `\n`，`\t`，`\r`，`\\`（转义 `\`）和 `\"` 进行了处理（在 `lab2/shell/src/utils.cpp` 的 `parse_escape` 中处理）。

### 管道

支持**多管道**，同时管道符 `|` 两侧可以不需要空格，也即支持 `ls | cat -n | grep 1` 和 `ls|cat -n|grep 1` （同 Bash）。

### 重定向

仅支持**基本**的文件重定向，重定向符 `<` `>` `>>` 两侧可以不需要空格（类似管道）。

### 处理 Ctrl + C

能够正确处理 Ctrl + C

### History

支持 History，在实验要求的基础上为了尽量与 Bash 一致，执行 `!!` 不会记录到 history 中，`!n` 则会记录到 history 中。

能够保存 history，文件位置为 `~/.shell_history`。

不支持上下键切换命令。

### 处理 Ctrl + D

能够正确处理 Ctrl + D。

### Alias

支持 alias。

### 变量

支持 `echo $SHELL`，`echo $HOME` 等。