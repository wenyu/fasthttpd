DISCLAIMER: DO NOT just clone this repository and submit it as your
own for your homework. Feel free to use it if you need a temporary
light-weight HTTP server. I did not use any provided skeleton code
provided by the course when I was implementing this, I built this
server from scratch.

-------------------------------------------------

CS290 / Project 3
Implmenting a Web-Server

Programmer:
Wenyu Zhang <zhangw@purdue.edu>

November 5, 2010
Purdue University

-------------------------------------------------

All features in the lab handout have been implemented, but with the
following modifications:

1. The default root directory of HTTP Service will be ./wwwroot/,
instead of ./http-root-dir.

2. The log will not be preserved across the runs. This is an intended
feature, because we will be interested to see how does each server
instance runs. To enable preserved log across the runs, please do the
following:

2.1. Open main.cc

2.2. Search for "char logFilePath[32]" and modify the initializer to
the file you prefer.

2.3. Search for "strcat(logFilePath, uusi())", and comment this
out. This statement should be the first line in main().

2.4. Recompile the project.


--------------------------------------------------
Extra features:

1. HTTP 206 Partial Content.

This makes it possible to download a huge file with a multi-threaded
downloader, or continue the downloading after you terminate the
downloading process in the middle. To test this: put a huge file in
./wwwroot/htdocs/, then use "wget -c" to download that file, and you
may terminate the wget program in the middle, and continue the
downloading again later.

2. Shared memory for Forking Server.

It is obvious that those statistical variables (min / max service time
and correspoding requests, requests) of server are not going to be
transferred back in any child processes to parent process easily, this
program does create shared memory between parent and children. Since
mutex locks won't work across processes, the program uses a spin lock
to resolve synchronization problems. (Code: http_service.cc)

3. Time out

The program will time out a connection if the connection has no
activities or *frozen* for a while. This prevents a created connection
occupies resources and does nothing. (Code: service.cc)

4. Keep-alive

The program supports the keep-alive option (but disabled by default),
so that a single connection can transfer multiple documents in a
single session. (Code: http_service.cc / http_service.h)

And the statistical result increases according to each request is
made. (i.e., a single session with multiple requests will modify the
statistical result several times.)

5. POST method

POST method is implemented in this web server.
