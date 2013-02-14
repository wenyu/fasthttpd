/*

iterative_server.h - Definition of Iterative Server

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#ifndef _ITERATIVE_SERVER_H_
#define _ITERATIVE_SERVER_H_

#include "config.h"
#include "server.h"

class IterativeServer : public Server {
public:
  IterativeServer();
  virtual ~IterativeServer();
  virtual int run();
};

#endif // _ITERATIVE_SERVER_H_
