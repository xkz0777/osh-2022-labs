# Lab3 Report

Name: 许坤钊

Student number: PB20111714

## 目录结构及简单的功能介绍

`chat_room_server` 是完善后的双人聊天室代码，支持发送非文本文件（直接遍历 `vector`，没有转成字符串），可以测试大文件发送。

`multithread_chat_room` 是基于多线程的多人聊天室，使用 `std::sync::mpsc::channel` 作为消息队列，`std::sync::{Arc, Mutex}` 作为互斥锁，不支持发送非文本文件，因为消息作为 `String` 封装到了 `struct Message` 里。

## 编译运行

`cargo run <port>` 即可
