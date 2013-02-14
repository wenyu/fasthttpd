/*

fork_server.h - Definition of Forking Server

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#ifndef _FORK_SERVER_H_
#define _FORK_SERVER_H_

#include "config.h"
#include "server.h"

class ForkServer : public Server {
public:
  ForkServer();
  virtual ~ForkServer();
  virtual int run();
};

#endif // _FORK_SERVER_H_
