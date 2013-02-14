/*

  server.h - General Server Interface Definition

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "service.h"

class Server {
public:
  Server();
  virtual ~Server();

  void initialize(int port, Service *origService, int sizeOfService);
  int startListen();
  void stopListen();
  int callServiceHandler(int sockfd, struct sockaddr_in clientIP);

  // Interface functions
  virtual int run() = 0;

protected:
  int _port, _sizeOfService;
  Service *_origService;
  struct sockaddr_in _serverIP;
  int _masterSocket;
  bool _initialized;
};

#endif // _SERVER_H_
