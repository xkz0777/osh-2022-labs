use io_uring::{opcode, types, IoUring};
use std::os::unix::io::AsRawFd;
use std::{fs, io};

fn add_fd_to_sq(fd: RawFd, op: io_uring::opcode, sq: &mut io_uring::Sq) -> io::Result<()> {
    let mut read_e = op::new(types::Fd(fd), ptr::null_mut(), ptr::null_mut)
        .build()
        .user_data(0);
    unsafe {
        sq.push(&read_e).unwrap();
    }
    Ok(())
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
    println!("Server listening on port {}", port);

    let mut ring = IoUring::new(32)?;
    let (submitter, mut sq, mut cq) = ring.split();
    Ok(())
}
