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
2. HTTP 1.1 layer (SEMI-COMPLETED)  
3. WebSocket layer  
4. TLS layer (to be merged with the TCP layer)  

TLS is debatable. It might be done with GnuTLS. Time will show.
# Current progress
- Support for both blocking and non blocking sockets, client and server  
- Helper functions to set corking, Nagle's algorithm, TCP's idle timeout  
- Epoll in combination with a highly optimized AVL tree to keep track of sockets and do asynchronous I/O on them  
- Connect, disconnect, send and receive data without any excessive functions and without anything excessive done in the background  
- Full control - you decide what you do with errors such as OOM and other critical ones; full access to the structures the underlying code is using; multiple handlers for events such as `onerror`, `onstart`, `onstop`, all called instantly when an event occurs and not in an asynchronous event loop (critical ones secured by a mutex without your interference to allow you to modify your sockets on go in critical situations, if needed), for instance making you able to handle an OOM error in the middle of an accepting thread and instantly resume normal execution without dropping the connection  
- Freedom - set sockets as non-blocking but do not bind them to an epoll instance; don't set up an `onmessage` socket handler if you don't care about incoming data (it will be taken care of for you)  
- Flexibility - set up as many threads `accept()`-ing connections as you like (or none at all), set the trade-off between speed and memory usage for the mentioned AVL tree by adjusting size of allocations  

There are functions to create HTTP requests and responses as well as parse them. The HTTP that the code supports is not standard-compliant - it is much stricter. For instance, by default it does not allow multiple whitespace before/after header names or/and values, as well as not allowing CRLF (stretching header values across multiple lines). This behavior, specified in the standard, is most commonly redundant and not practiced unless specified otherwise by the documentation of the code.  
The code does not contain any compression algorithm, nor support for chunked transfer (yet?). Besides that, all error codes are available, as well as good writing/reading from a HTTP stream - you can parse only up to some moment, save the progress and then reinvoke the parsing function to either, again, proceed up to some point, or finish off processing the message. This strategy allows to detect undesired messages without using too much computing power - if, for instance, we, as a server, only accept GET requests, and we stumble upon any other method, we can read only the first few bytes instead of parsing the whole message (the `HTTP_PARSE_METHOD` flag does exactly this) to construct a response. Additionally, there are settings associated with parsing: such as maximum header name length, maximum header value length, maximum path length (request URI), maximum amount of headers, maximum length of reason phrase (in response messages).  
Websocket protocol support is yet to come.  
# Contribution
Bug issues are welcome.
# License
Apache 2.0