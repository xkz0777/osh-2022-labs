use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::{env, process, thread};

// source: https://riptutorial.com/rust/example/4404/a-simple-tcp-client-and-server-application--echo
fn handler_chat(mut tcp_read: TcpStream, tcp_write: &mut TcpStream) {
    let prompt = "Message: ".as_bytes().to_vec();
    let mut message = prompt.clone();
    let mut buffer = [0_u8; 1024];
    while let Ok(recieved) = tcp_read.read(&mut buffer) {
        if recieved == 0 {
            // 不发空消息
            continue;
        } else {
            buffer[0..recieved].iter().for_each(|x| {
                message.push(*x);
                if *x == b'\n' {
                    tcp_write
                        .write_all(message.as_slice()) // write_all 处理大文件
                        .expect("TCP write error");
                    message = prompt.clone();
                }
            });
        }
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
    println!("Server listening on port {}", port);

    let mut tcp1 = listener.accept().expect("Tcp accept error");
    let mut tcp2 = listener.accept().expect("Tcp accept error");

    // 创建两个线程，一个线程处理 tcp1，一个线程处理 tcp2，创建前先 clone 防止因为 move 丢失所有权
    let tcp_clone1 = tcp1.0.try_clone().expect("Clone failed");
    let tcp_clone2 = tcp2.0.try_clone().expect("Clone failed");

    let thread1 = thread::spawn(move || handler_chat(tcp_clone1, &mut tcp2.0));
    let thread2 = thread::spawn(move || handler_chat(tcp_clone2, &mut tcp1.0));

    thread1.join().expect("Thread 1 join error");
    thread2.join().expect("Thread 2 join error");
}
