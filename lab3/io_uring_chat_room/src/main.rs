extern crate io_uring;
extern crate libc;

use io_uring::{opcode, types, IoUring, SubmissionQueue};
use std::collections::HashMap;
use std::net::{TcpListener, TcpStream};
use std::os::unix::io::AsRawFd;
use std::os::unix::prelude::*;
use std::{env, io, process, ptr};

#[derive(Debug, Clone)]
enum Token {
    Accept,
    Poll {
        fd: RawFd,
    },
    Read {
        fd: RawFd,
        sender_id: i32,
        receiver_id: i32,
    },
    Write {
        fd: RawFd,
        receiver_id: i32,
        len: usize,
        offset: usize,
    },
}

fn add_accept(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32) {
    let accept_e = opcode::Accept::new(types::Fd(fd), ptr::null_mut(), ptr::null_mut())
        .build()
        .user_data(user_data as _);

    unsafe {
        sq.push(&accept_e).expect("Failed to add accept");
    }
    sq.sync();
}

fn add_poll(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32) {
    let poll_e = opcode::PollAdd::new(types::Fd(fd), libc::POLLIN as _)
        .build()
        .user_data(user_data as _);

    unsafe {
        sq.push(&poll_e).expect("Failed to add poll");
    }
    sq.sync();
}

/// 从 socket fd 读到 buffer 里
fn add_read(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32, buffer: &mut [u8; 1024]) {
    let read_e = opcode::Recv::new(types::Fd(fd), buffer.as_mut_ptr(), buffer.len() as _)
        .build()
        .user_data(user_data as _);
    println!("Add read!");
    unsafe {
        sq.push(&read_e).expect("Failed to add read");
    }
    sq.sync();
}

fn add_write(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32, buffer: &[u8; 1024], len: i32) {
    let write_e = opcode::Send::new(types::Fd(fd), buffer.as_ptr(), len as _)
        .build()
        .user_data(user_data as _);

    unsafe {
        sq.push(&write_e).expect("Failed to add write");
    }
    sq.sync();
}

fn fd_to_tcpstream(fd: i32) -> TcpStream {
    unsafe { TcpStream::from_raw_fd(fd) }
}

fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() > 2 {
        println!("Invalid number of arguments");
        process::exit(1);
    }
    if args.len() == 1 {
        println!("Usage: cargo run <port>");
        process::exit(1);
    }

    let port = &args[1];
    let addr = &format!("127.0.0.1:{}", port);
    // server 开始监听
    let listener = TcpListener::bind(addr)?;
    let listener_raw_fd = listener.as_raw_fd();

    println!("Server listening on port {}", port);

    let mut ring = IoUring::new(32)?;
    let (sm, mut sq, mut cq) = ring.split();

    // listener 加入 sq
    add_accept(&mut sq, listener_raw_fd, 100);

    let mut id = 0;
    let mut total = 1;
    let mut fd_map: HashMap<i32, TcpStream> = HashMap::new();
    let mut token_map: HashMap<i32, Token> = HashMap::new();
    let mut buffer_map = HashMap::new();

    // let prompt = "Message: ".as_bytes().to_vec();

    token_map.insert(100, Token::Accept);

    // 开始监听 cq 队列
    loop {
        // 最小完成一个时返回
        sm.submit_and_wait(total)?;
        cq.sync();

        for cqe in &mut cq {
            // 收割 cqe
            let ret = cqe.result();
            let data = cqe.user_data() as i32;
            if ret < 0 {
                eprintln!(
                    "token {:?} error: {:?}",
                    token_map.get(&data),
                    io::Error::from_raw_os_error(-ret)
                );
                continue;
            }

            let token = token_map.get(&data).unwrap();
            let token_clone = (*token).clone();
            match token_clone {
                // 添加新的 client
                Token::Accept => {
                    println!("Accept!");
                    id += 1;
                    total += 1;
                    let fd = ret;
                    let stream = fd_to_tcpstream(fd);
                    stream.set_nonblocking(true)?;

                    println!(
                        "Client connnectd: {}, raw_fd: {}",
                        stream.peer_addr().unwrap(),
                        fd
                    );

                    fd_map.insert(id, stream);
                    add_poll(&mut sq, fd, id);
                    token_map.insert(id, Token::Poll { fd });
                    let buffer = [0_u8; 1024];
                    buffer_map.insert(id, buffer);
                    add_accept(&mut sq, listener_raw_fd, 100);
                }
                // 从 client 里读取到各个 buffer 里
                Token::Poll { fd } => {
                    println!("Poll! fd: {}", fd);
                    let sender_id = data;
                    for (id, stream) in &fd_map {
                        println!("looping: id: {}, sender_id: {}", id, sender_id);
                        if *id != sender_id {
                            let mut buffer = buffer_map.get_mut(id).unwrap();
                            add_read(&mut sq, fd, *id, &mut buffer);
                            token_map.insert(
                                *id,
                                Token::Read {
                                    fd: stream.as_raw_fd(),
                                    sender_id: sender_id,
                                    receiver_id: *id,
                                },
                            );
                        }
                    }
                }
                Token::Read {
                    fd,
                    sender_id,
                    receiver_id,
                } => {
                    println!("Read! sender: {}, receiver: {}", sender_id, receiver_id);
                    if ret == 0 {
                        total -= 1;
                        token_map.remove(&data);
                        buffer_map.remove(&data);
                        println!("shutdown");
                        unsafe {
                            libc::close(fd);
                        }
                    } else {
                        let len = ret;
                        let buffer = buffer_map.get(&receiver_id).unwrap();
                        println!(
                            "Client {} sent message {} to {}",
                            sender_id,
                            std::str::from_utf8(buffer.as_slice()).expect("Non UTF-8 message"),
                            receiver_id
                        );
                        add_write(&mut sq, fd, data, &buffer, len);
                        token_map.insert(
                            data,
                            Token::Write {
                                fd,
                                receiver_id,
                                len: len as usize,
                                offset: 0,
                            },
                        );
                    }
                }
                Token::Write {
                    fd,
                    receiver_id,
                    len,
                    offset,
                } => {
                    println!("Write! receiver: {}", receiver_id);
                    let written_len = ret as usize;
                    if offset + written_len >= len {
                        token_map.insert(data, Token::Poll { fd });

                        add_poll(&mut sq, fd, data);
                    } else {
                        let offset = offset + written_len;
                        let len = len - offset;

                        let buf = &buffer_map.get(&receiver_id).unwrap()[offset..];

                        token_map.insert(
                            data,
                            Token::Write {
                                fd,
                                receiver_id,
                                len,
                                offset,
                            },
                        );

                        opcode::Write::new(types::Fd(fd), buf.as_ptr(), len as _)
                            .build()
                            .user_data(data as _);
                    };
                }
            }
            sq.sync();
            println!("{:?}", token_map);
        }
    }
}
