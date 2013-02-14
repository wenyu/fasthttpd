/*

pool_server.h - Definition of Pool Server

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#ifndef _POOL_SERVER_H_
#define _POOL_SERVER_H_

#include "config.h"
#include "server.h"
#include <pthread.h>

#define PARALLEL_INSTANCES 100

class PoolServer : public Server {
public:
  PoolServer();
  virtual ~PoolServer();
  virtual int run();
  void threadRun();

private:
  pthread_mutex_t acceptLock;
};

#endif // _POOL_SERVER_H_
