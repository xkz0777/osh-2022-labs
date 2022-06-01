extern crate libc;

use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::TcpListener;
use std::os::unix::io::{AsRawFd, RawFd};
use std::{env, io, mem, process, ptr, time};

pub struct FdSet(libc::fd_set);

impl FdSet {
    pub fn new() -> FdSet {
        unsafe {
            let mut raw_fd_set = mem::MaybeUninit::<libc::fd_set>::uninit();
            libc::FD_ZERO(raw_fd_set.as_mut_ptr());
            FdSet(raw_fd_set.assume_init())
        }
    }
    pub fn clear(&mut self, fd: RawFd) {
        unsafe { libc::FD_CLR(fd, &mut self.0) }
    }
    pub fn set(&mut self, fd: RawFd) {
        unsafe { libc::FD_SET(fd, &mut self.0) }
    }
    pub fn is_set(&mut self, fd: RawFd) -> bool {
        unsafe { libc::FD_ISSET(fd, &mut self.0) }
    }
}

fn to_fdset_ptr(opt: Option<&mut FdSet>) -> *mut libc::fd_set {
    match opt {
        None => ptr::null_mut(),
        Some(&mut FdSet(ref mut raw_fd_set)) => raw_fd_set,
    }
}
fn to_ptr<T>(opt: Option<&T>) -> *const T {
    match opt {
        None => ptr::null::<T>(),
        Some(p) => p,
    }
}

pub fn select(
    nfds: libc::c_int,
    readfds: Option<&mut FdSet>,
    writefds: Option<&mut FdSet>,
    errorfds: Option<&mut FdSet>,
    timeout: Option<&libc::timeval>,
) -> io::Result<i32> {
    match unsafe {
        libc::select(
            nfds,
            to_fdset_ptr(readfds),
            to_fdset_ptr(writefds),
            to_fdset_ptr(errorfds),
            to_ptr::<libc::timeval>(timeout) as *mut libc::timeval,
        )
    } {
        -1 => Err(io::Error::last_os_error()),
        res => Ok(res),
    }
}

pub fn make_timeval(duration: time::Duration) -> libc::timeval {
    libc::timeval {
        tv_sec: duration.as_secs() as i64,
        tv_usec: duration.subsec_micros() as i64,
    }
}

fn main() {
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
    let listener = TcpListener::bind(addr).expect("Tcp bind error");
    listener
        .set_nonblocking(true)
        .expect("Cannot set non-blocking");
    println!("Server listening on port {}", port);

    // 创建 fd_set
    let mut fd_set = FdSet::new();

    let listener_raw_fd = listener.as_raw_fd();
    let mut max_fd = listener_raw_fd;
    fd_set.set(listener_raw_fd);

    let mut client_num = 0;

    // client_id 到 TcpStream 的哈希表
    let mut fd_map = HashMap::new();

    let prompt = "Message: ".as_bytes().to_vec();
    let mut message = prompt.clone();
    let mut buffer = [0_u8; 1024];

    while let Ok(res) = select(max_fd + 1, Some(&mut fd_set), None, None, None) {
        println!("Select result: {}", res);
        if fd_set.is_set(listener_raw_fd) {
            if let Ok((client, address)) = listener.accept() {
                client
                    .set_nonblocking(true)
                    .expect("Failed to set non-blocking");
                println!("Client connected: {}", address);
                let raw_fd = client.as_raw_fd();
                client_num += 1;
                fd_map.insert(client_num, client);
                max_fd = max_fd.max(raw_fd);
                fd_set.set(raw_fd);
            }
        } else {
            let mut quit = false;
            let mut quit_id = -1;
            for (i, mut stream) in &fd_map {
                if fd_set.is_set(stream.as_raw_fd()) {
                    while let Ok(recieved) = stream.read(&mut buffer) {
                        println!(
                            "Client {} sent message: {}",
                            stream.peer_addr().unwrap(),
                            std::str::from_utf8(buffer.as_slice()).unwrap()
                        );
                        if recieved == 0 {
                            quit_id = *i;
                            quit = true;
                            break;
                        } else {
                            buffer[0..recieved].iter().for_each(|x| {
                                message.push(*x);
                                if *x == b'\n' {
                                    for (id, mut target) in &fd_map {
                                        if *id != *i {
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
            if quit {
                let to_be_removed = fd_map.get(&quit_id).unwrap();
                println!("Removed Client {}", to_be_removed.peer_addr().unwrap());
                fd_map.remove(&quit_id);
            }
        }
        fd_set = FdSet::new();
        for (_, fd) in &fd_map {
            fd_set.set(fd.as_raw_fd());
        }
        fd_set.set(listener_raw_fd);
    }
}
