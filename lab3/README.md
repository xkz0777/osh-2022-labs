# Lab3 Report

## 目录结构及简单的功能介绍

`chat_room_server` 是完善后的双人聊天室代码，支持发送非文本文件（直接遍历 `vector`，没有转成字符串），可以测试大文件发送。

`multithread_chat_room` 是基于多线程的多人聊天室，使用 `std::sync::mpsc::channel` 作为消息队列，`std::sync::{Arc, Mutex}` 作为互斥锁。

注：不支持发送非文本文件，因为消息作为 `String` 封装到了 `struct Message` 里，把存有 `u8` 的数组转成 `String` 需要确定编码。

`select_chat_room` 是用 `select` 完成的多人聊天室。

`epoll_chat_room` 是用 `epoll` 完成的多人聊天室。

`io_uring_chat_room` 是用 `io_uring` 完成的多人聊天室。

## 编译运行

全部 `cargo run <port>` 即可