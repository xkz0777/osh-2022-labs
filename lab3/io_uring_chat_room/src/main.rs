extern crate io_uring;
extern crate libc;

use io_uring::squeue::PushError;
use io_uring::{opcode, types, IoUring, SubmissionQueue};
use std::collections::HashMap;
use std::net::{TcpListener, TcpStream};
use std::os::unix::io::AsRawFd;
use std::os::unix::prelude::*;
use std::{env, io, process, ptr};

#[derive(Copy, Clone, Eq, Hash, PartialEq)]
enum Token {
    Accept,
    Poll { fd: RawFd },
    Read { fd: RawFd, buffer_id: i32 },
    Write { fd: RawFd, buffer_id: i32 },
}

fn add_accept(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32) -> Result<(), PushError> {
    let accept_e = opcode::Accept::new(types::Fd(fd), ptr::null_mut(), ptr::null_mut())
        .build()
        .user_data(user_data as u64);

    unsafe {
        sq.push(&accept_e)?;
    }
    Ok(())
}

fn add_read(
    sq: &mut SubmissionQueue,
    fd: RawFd,
    user_data: i32,
    buffer: &mut [u8; 1024],
) -> Result<(), PushError> {
    let read_e = opcode::Recv::new(types::Fd(fd), buffer.as_mut_ptr(), buffer.len() as u32)
        .build()
        .user_data(user_data as u64);

    unsafe {
        sq.push(&read_e)?;
    }
    Ok(())
}

fn add_write(
    sq: &mut SubmissionQueue,
    fd: RawFd,
    user_data: i32,
    buffer: &[u8; 1024],
) -> Result<(), PushError> {
    let write_e = opcode::Send::new(types::Fd(fd), buffer.as_ptr(), buffer.len() as u32)
        .build()
        .user_data(user_data as u64);

    unsafe {
        sq.push(&write_e)?;
    }
    Ok(())
}

fn write(fd: RawFd, buffer: &[u8; 1024]) {
    opcode::Write::new(types::Fd(fd), buffer.as_ptr(), buffer.len() as u32)
        .build()
        .user_data(0);
}

fn add_poll(sq: &mut SubmissionQueue, fd: RawFd, user_data: i32) -> Result<(), PushError> {
    let poll_e = opcode::PollAdd::new(types::Fd(fd), libc::POLLIN as u32)
        .build()
        .user_data(user_data as u64);

    unsafe {
        sq.push(&poll_e)?;
    }
    Ok(())
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
    add_accept(&mut sq, listener_raw_fd, 100).expect("Failed to add accept");
    sq.sync();

    let mut id = 0;
    let mut fd_map = HashMap::new();
    let mut token_map: HashMap<i32, Token> = HashMap::new();
    // let mut buffer_map = HashMap::new();

    // let prompt = "Message: ".as_bytes().to_vec();
    let mut buffer = [0_u8; 1024];

    token_map.insert(100, Token::Accept);

    // 开始监听 cq 队列
    loop {
        // 最小完成一个时返回
        sm.submit_and_wait(1)?;
        cq.sync();
        for cqe in &mut cq {
            // 收割 cqe
            let ret = cqe.result();
            if ret < 0 {
                eprintln!("Error: {}", io::Error::from_raw_os_error(ret));
                continue;
            }
            println!("cq complete one");
            let data = cqe.user_data() as i32;
            let token = token_map.get(&data).unwrap();
            let token_clone = *token;
            match token_clone {
                // 添加新的 client
                Token::Accept => {
                    id += 1;
                    let fd = cqe.result();
                    let stream = fd_to_tcpstream(fd);
                    stream
                        .set_nonblocking(true)
                        .expect("Failed to set non-blocking");

                    println!("Client connnectd: {}", stream.peer_addr().unwrap());

                    fd_map.insert(id, stream.try_clone().unwrap());
                    add_poll(&mut sq, fd, id).expect("Failed to add poll");
                    token_map.insert(id, Token::Poll { fd });
                    // let buffer = [0_u8; 1024];
                    // buffer_map.insert(id, buffer);

                    add_accept(&mut sq, listener_raw_fd, 100).expect("Failed to add accept");
                    // 再把 listener 加进去
                }
                // 从 client 里读取到各个 buffer 里
                Token::Poll { fd } => {
                    for (id, stream) in &fd_map {
                        if *id != data {
                            // let mut buffer = buffer_map.get_mut(id).unwrap();
                            add_read(&mut sq, fd, data, &mut buffer).expect("Failed to add read");
                            println!(
                                "Client {} sent message: {}",
                                fd,
                                std::str::from_utf8(buffer.as_slice())
                                    .expect("Error: non UTF-8 Message")
                            );
                            token_map.insert(
                                data,
                                Token::Read {
                                    fd: stream.as_raw_fd(),
                                    buffer_id: data,
                                },
                            );
                        }
                    }
                }
                Token::Read { fd, buffer_id } => {
                    add_write(&mut sq, fd, data, &buffer).expect("Failed to add write");
                    token_map.insert(data, Token::Write { fd, buffer_id });
                }
                Token::Write { fd, buffer_id: _ } => {
                    write(fd, &buffer);
                }
            }
            sq.sync();
        }
    }
}
