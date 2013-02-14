/*

thread_server.h - Definition of Thread Server

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#ifndef _THREAD_SERVER_H_
#define _THREAD_SERVER_H_

#include "config.h"
#include "server.h"

class ThreadServer : public Server {
public:
  ThreadServer();
  virtual ~ThreadServer();
  virtual int run();
};

#endif // _THREAD_SERVER_H_
