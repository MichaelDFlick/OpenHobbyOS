# Pipes and Sockets

OpenHobbyOS supports inter-process communication through Unix pipes and sockets.

## Pipes

Pipes provide one-way communication between processes. A pipe has a read endpoint and a write endpoint. Data written to the write endpoint can be read from the read endpoint in FIFO order.

pipe() creates a pipe and returns two file descriptors: one for reading and one for writing. Data written to the write end is buffered in kernel memory. If the buffer is full, the writing process blocks until data is read from the read end.

When all write ends are closed, reads on the read end return EOF. When all read ends are closed, writes on the write end return an error.

Pipes are used extensively by the shell for connecting the output of one command to the input of another.

## Unix Domain Sockets

Unix domain sockets provide bidirectional communication between processes on the same machine. They use file system paths as addresses.

socket() creates a socket. bind() associates it with a path. listen() marks it as a server socket. accept() waits for and accepts incoming connections. connect() initiates a connection to a server socket.

Data sent through a Unix domain socket is delivered in order and without message boundaries (stream sockets).

## BSD Sockets

The kernel also provides BSD socket support for network communication. This includes AF_INET sockets for TCP/IP networking through the lwIP stack.

socket(), bind(), connect(), listen(), accept(), send(), recv(), sendto(), recvfrom(), shutdown(), setsockopt(), and getsockopt() are all supported for network sockets.

## Socket Implementation

Sockets are implemented as a special file type in the VFS. When a socket file descriptor is read or written, the VFS dispatches to the socket layer which handles the data transfer.

Unix domain sockets use a kernel-managed buffer for data transfer between endpoints. The buffer is allocated on demand and freed when both endpoints are closed.
