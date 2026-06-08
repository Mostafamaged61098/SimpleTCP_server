# TCP Server / Client in C
A lightweight, terminal-based TCP client-server application written in C, demonstrating core socket programming concepts including connection handling, message exchange, and graceful disconnection.

## Features

Establishes a TCP connection between a server and one or more clients
Bidirectional message exchange over a local or network socket
Lightweight with zero external dependencies — pure C and POSIX sockets
Clean, readable code suitable for learning and extension

## Project Structure
<br/>|
<br/>|── Server.c       # TCP server — listens and handles incoming connections
<br/>|── Client.c       # TCP client — connects to the server and sends messages
<br/>|── README.md

## Requirements

GCC compiler
Linux / macOS (or any POSIX-compliant OS)
No external libraries required

## Build
Compile each program separately:
<br/>gcc Server.c -o Server
<br/>gcc Client.c -o Client

## Usage
1. Start the server (in one terminal):
<br/>bash ./Server
<br/>The server will start listening for incoming connections.
2. Connect a client (in another terminal):
<br/>bash ./Client
<br/>The client will connect to the server and you can begin exchanging messages.

//If running on different machines, update the IP address in Client.c to point to the server's IP.


## How It Works
Client                        Server
<br/>  |-------- connect() ---------> |
<br/>  |-------- send(msg) ---------> |
<br/>  | <------- recv(msg) --------- |
<br/>  |-------- close() -----------> |

The Server creates a socket, binds to a port, and calls listen() to wait for connections
The Client creates a socket and calls connect() to reach the server
Once connected, both sides can send() and recv() messages
Either side can close the connection gracefully


## Concepts Demonstrated

BSD socket API (socket, bind, listen, accept, connect)
TCP/IP connection lifecycle
Client-server architecture
POSIX system calls in C


<br/> ## Author
<br/>[Mostafa Maged]
<br/>https://github.com/Mostafamaged61098
<br/>m.mostafamostafa.mm@gmail.com
