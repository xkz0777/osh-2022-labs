use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::{env, process, thread};

struct Message {
    client_id: usize, // 消息接收者的 id
    content: String,
}

impl Message {
    fn new(client_id: usize, content: String) -> Message {
        Message { client_id, content }
    }
}

fn handle_send(
    mut tcp_read: TcpStream,
    sender_id: usize, // 发信息的 id
    clients_mutex_clone: Arc<Mutex<Vec<Option<TcpStream>>>>,
    sender: Sender<Message>,
) {
    let prompt = "Message: ".as_bytes().to_vec();
    let mut message = prompt.clone();
    let mut buffer = [0_u8; 1024];
    while let Ok(recieved) = tcp_read.read(&mut buffer) {
        if recieved == 0 {
            // 客户端退出
            break;
        } else {
            buffer[0..recieved].iter().for_each(|x| {
                message.push(*x);
                if *x == b'\n' {
                    let message_str =
                        String::from_utf8(message.clone()).expect("Invalid UTF-8 message");
                    let clients = clients_mutex_clone.lock().unwrap();
                    for (i, client) in clients.iter().enumerate() {
                        if client.is_some() && i != sender_id {
                            sender.send(Message::new(i, message_str.clone())).unwrap();
                        }
                    }
                    drop(clients);
                    message = prompt.clone();
                }
            });
        }
    }
    let mut clients = clients_mutex_clone.lock().unwrap();
    clients[sender_id] = None;
}

fn handle_receive(receiver: Receiver<Message>, clients_mutex: Arc<Mutex<Vec<Option<TcpStream>>>>) {
    while let Ok(message) = receiver.recv() {
        let mut clients = clients_mutex.lock().unwrap();
        clients
            .get_mut(message.client_id)
            .unwrap()
            .as_mut()
            .unwrap()
            .write_all(message.content.as_bytes())
            .unwrap();
        drop(clients);
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

    // 开消息队列
    let (sender, receiver) = mpsc::channel::<Message>();

    // client 队列
    let clients = vec![];
    let clients_mutex = Arc::new(Mutex::new(clients));
    let clients_mutex_clone = Arc::clone(&clients_mutex);

    // 创一个线程处理信息接收
    thread::spawn(move || {
        handle_receive(receiver, clients_mutex_clone);
    });

    // 等待 client 连接
    loop {
        if let Ok((client, _address)) = listener.accept() {
            println!("Client connected: {}", client.peer_addr().unwrap());
            // listener 是非阻塞的，client 需要设回阻塞，否则 `handle_send` 里的 while 循环会在连接后立即退出
            client.set_nonblocking(false).expect("Cannot set blocking");
            // 加入客户队列
            let clients_mutex_clone = Arc::clone(&clients_mutex);
            let mut clients_guard = clients_mutex_clone.lock().unwrap();
            clients_guard.push(Some(client.try_clone().unwrap()));
            let sender_id = clients_guard.len() - 1;
            drop(clients_guard);

            // 创建一个线程来处理 client
            let sender_client = sender.clone();
            let clients_mutex_clone = Arc::clone(&clients_mutex);

            thread::spawn(move || {
                handle_send(client, sender_id, clients_mutex_clone, sender_client);
            });
        }
    }
}
