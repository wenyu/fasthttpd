/*

  service.h - General Service Interface Definition

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <cstdio>
#include <pthread.h>
#include <netinet/in.h>

#include "config.h"

class Service {
public:
  Service();
  virtual ~Service();
  virtual int initialize();
  int processRequest(int sockfd, struct sockaddr_in clientIP); // This initializes threads, and calls service handler
  virtual Service* clone() = 0;
  virtual int serviceHandler() = 0;
  virtual int timeOutHandler();
  void setTimeOutPeriod(int timeOut);

public:
  int _timeOut;
  pthread_t thrHandler;

protected:
  bool _initialized, _requestProcessed;
  int sockfd; 
  FILE* fsock;
  struct sockaddr_in clientIP;
};

#endif // _SERVICE_H_
