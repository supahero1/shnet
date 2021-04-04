# net
Networking code for any of my future projects. Work in progress.  
# Requirements
Posix-compliant system, GCC environment of a reasonable version (glibc >= 2.11).
# Building
`make`  
Look at `Makefile` for more details about compiling process if you wish.
# Future vision
I can see 4 steps associated with this project:  
1. Eventing, timers and TCP layer (DONE)  
2. HTTP layer (TO BE DONE)  
3. WebSocket layer  
4. TLS layer (to be merged with the TCP layer)  
At first I was thinking about doing TLS straightforwardly with OpenSSL or WolfSSL or any other TLS library, but then I realised it might be overkill. I decided to delay it until the end and write it myself (probably with some help of a library providing optimised cryptographic functions, maybe OpenSSL).
# Current progress
- Support for both blocking and non blocking sockets, client and server  
- Helper functions to set corking, Nagle's algorithm, TCP's idle timeout  
- Epoll in combination with a highly optimised AVL tree to keep track of sockets and do asynchronous I/O on them  
- Connect, disconnect, send and receive data without any excessive functions and without anything excessive done in the background  
- Full control - you decide what you do with errors such as OOM and other critical ones; full access to the structures the underlying code is using; multiple handlers for events such as `onerror`, `onstart`, `onstop`, all called instantly when an event occurs and not in an asynchronous event loop (critical ones secured by a mutex without your interference to allow you to modify your sockets on go in critical situations, if needed)  
- Freedom - set sockets as non-blocking but do not bind them to an epoll instance; don't set up an `onmessage` socket handler if you don't care about incoming data (it will be taken care of for you)  
- Flexibility - set up as many threads `accept()`-ing connections as you like (or none at all), set the trade-off between speed and memory usage for the mentioned AVL tree by adjusting size of allocations  

Right now the functionality is only limited to TCP (no UDP btw), but as I go through next steps there will obviously be more stuff to do.  
You can already experiment by creating a connection and then simply sending a HTTP GET request or anything else you like, or by setting up a local server and then creating connections to the server.  
# Contribution
Bug issues are welcome.
# License
Apache 2.0