# Lab3 Report

## 目录结构及简单的功能介绍

`chat_room_server` 是完善后的双人聊天室代码。

`multithread_chat_room` 是基于多线程的多人聊天室，使用 `std::sync::mpsc::channel` 作为消息队列，`std::sync::{Arc, Mutex}` 作为互斥锁。

## 编译运行

`cargo run <port>` 即可