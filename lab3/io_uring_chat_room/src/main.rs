extern crate io_uring;
extern crate libc;

use io_uring::{opcode, types, IoUring, SubmissionQueue};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::os::unix::io::AsRawFd;
use std::os::unix::prelude::*;
use std::{env, io, process, ptr};

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
    let mut fd_map: HashMap<i32, TcpStream> = HashMap::new();

    let prompt = "Message: ".as_bytes().to_vec();
    let mut buffer = [0_u8; 1024];
    let mut message = prompt.clone();

    // 开始监听 cq 队列
    loop {
        // 最小完成一个时返回
        match sm.submit_and_wait(1) {
            Ok(_) => (),
            Err(ref err) if err.raw_os_error() == Some(libc::EBUSY) => (),
            Err(err) => return Err(err),
        }
        cq.sync();

        for cqe in &mut cq {
            // 收割 cqe
            let ret = cqe.result();
            let data = cqe.user_data() as i32;
            if ret < 0 {
                eprintln!("Error: {:?}", io::Error::from_raw_os_error(-ret));
                continue;
            }

            match data {
                100 => {
                    // 添加新的 client
                    println!("Accept!");
                    id += 1;
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
                    add_accept(&mut sq, listener_raw_fd, 100);
                }
                other => {
                    let mut stream = fd_map.get(&other).unwrap();
                    let fd = stream.as_raw_fd();
                    let mut quit = false;
                    while let Ok(received) = stream.read(&mut buffer) {
                        println!(
                            "Client {} sent message: {}",
                            stream.peer_addr().unwrap(),
                            std::str::from_utf8(buffer.as_slice()).unwrap()
                        );
                        if received == 0 {
                            println!("Removed Client: {}", stream.peer_addr().unwrap());
                            fd_map.remove(&other);
                            quit = true;
                            break;
                        } else {
                            buffer[0..received].iter().for_each(|x| {
                                message.push(*x);
                                if *x == b'\n' {
                                    for (id, mut target) in &fd_map {
                                        if *id != data {
                                            println!("Sender: {}, receiver: {}", data, id);
                                            target.write_all(message.as_slice()).unwrap();
                                        }
                                    }
                                    message = prompt.clone();
                                }
                            });
                        }
                    }
                    if !quit {
                        add_poll(&mut sq, fd, data);
                    }
                }
            }
        }
    }
}
