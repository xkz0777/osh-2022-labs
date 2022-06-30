extern crate libc;

use libc::epoll_event;
use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::TcpListener;
use std::os::unix::io::{AsRawFd, RawFd};
use std::{env, io, process};

// source: https://www.zupzup.org/epoll-with-rust/
// wrap the libc syscall in an unsafe block
macro_rules! syscall {
    ($fn: ident ( $($arg: expr),* $(,)* ) ) => {{
        let res = unsafe { libc::$fn($($arg, )*) };
        if res == -1 {
            Err(std::io::Error::last_os_error())
        } else {
            Ok(res)
        }
    }};
}

fn epoll_create() -> io::Result<RawFd> {
    let fd = syscall!(epoll_create1(0))?;
    if let Ok(flags) = syscall!(fcntl(fd, libc::F_GETFD)) {
        syscall!(fcntl(fd, libc::F_SETFD, flags | libc::FD_CLOEXEC))?;
    }

    Ok(fd)
}

fn add_interest(epoll_fd: RawFd, fd: RawFd, mut event: libc::epoll_event) -> io::Result<()> {
    syscall!(epoll_ctl(epoll_fd, libc::EPOLL_CTL_ADD, fd, &mut event))?;
    Ok(())
}

fn modify_interest(epoll_fd: RawFd, fd: RawFd, mut event: libc::epoll_event) -> io::Result<()> {
    syscall!(epoll_ctl(epoll_fd, libc::EPOLL_CTL_MOD, fd, &mut event))?;
    Ok(())
}

fn remove_interest(epoll_fd: RawFd, fd: RawFd) -> io::Result<()> {
    syscall!(epoll_ctl(
        epoll_fd,
        libc::EPOLL_CTL_DEL,
        fd,
        std::ptr::null_mut()
    ))?;
    Ok(())
}

const READ_FLAGS: i32 = libc::EPOLLIN;

fn listener_read_event(id: u64) -> epoll_event {
    epoll_event {
        events: READ_FLAGS as u32,
        u64: id,
    }
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
    listener.set_nonblocking(true)?;
    println!("Server listening on port {}", port);

    let listener_raw_fd = listener.as_raw_fd();

    let mut id: u64 = 0;
    let mut fd_map = HashMap::new();

    // 获得 epoll 事件队列的 fd
    let epoll_fd = epoll_create()?;

    add_interest(epoll_fd, listener_raw_fd, listener_read_event(100))?;
    let prompt = "Message: ".as_bytes().to_vec();
    let mut message = prompt.clone();
    let mut buffer = [0_u8; 1024];

    // 创建事件队列
    let mut events: Vec<epoll_event> = Vec::with_capacity(32);

    loop {
        events.clear();
        let res = match syscall!(epoll_wait(
            epoll_fd,
            events.as_mut_ptr() as *mut epoll_event,
            32,
            -1 as libc::c_int,
        )) {
            Ok(v) => v,
            Err(e) => panic!("Error during epoll wait: {}", e),
        };
        /*  Once called, epoll_wait will block until one of three things happens:
        - A file descriptor, which we registered interest in has an event
        - A signal handler interrupts the call
        - The timeout expires*/

        // safe as long as the kernel does nothing wrong - copied from mio
        unsafe { events.set_len(res as usize) };

        for event in &events {
            match event.u64 {
                100 => {
                    // 0 是我们的服务器 id，因此这里是新的 client 连接
                    match listener.accept() {
                        Ok((stream, addr)) => {
                            stream.set_nonblocking(true)?;
                            println!("Client connected: {}", addr);
                            id += 1;
                            add_interest(epoll_fd, stream.as_raw_fd(), listener_read_event(id))?;
                            fd_map.insert(id, stream);
                        }
                        Err(e) => eprintln!("Couldn't accept: {}", e),
                    }
                    modify_interest(epoll_fd, listener_raw_fd, listener_read_event(100))?;
                }
                other => {
                    // id 为 other 的 client 发来消息
                    let mut stream = fd_map.get(&other).unwrap();
                    while let Ok(received) = stream.read(&mut buffer) {
                        println!(
                            "Client {} sent message: {}",
                            stream.peer_addr().unwrap(),
                            std::str::from_utf8(buffer.as_slice()).unwrap()
                        );
                        if received == 0 {
                            remove_interest(epoll_fd, stream.as_raw_fd())?;
                            println!("Removed Client {}", stream.peer_addr().unwrap());
                            fd_map.remove(&other);
                            break;
                        } else {
                            buffer[0..received].iter().for_each(|x| {
                                message.push(*x);
                                if *x == b'\n' {
                                    for (id, mut target) in &fd_map {
                                        if *id != other {
                                            target.write_all(message.as_slice()).unwrap();
                                        }
                                    }
                                    message = prompt.clone();
                                }
                            });
                        }
                    }
                }
            }
        }
    }
}
