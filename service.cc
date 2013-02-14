/*

  service.cc - General Service Interface

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#include <cassert>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "service.h"

Service::Service() {
  _initialized = false;
  _requestProcessed = false;
  sockfd = -1;
  fsock = NULL;
  memset(&clientIP, 0, sizeof(clientIP));
}

Service::~Service() {}

int Service::initialize() {
  _initialized = true;
}

int Service::processRequest( int sockfd, struct sockaddr_in clientIP ) {
  // Prevent reentry or recalling the same service object
  assert( ! _requestProcessed );
  _requestProcessed = true;

  this->sockfd = sockfd;
  if ( ! (fsock = fdopen(sockfd, "a+")) ) {
    perror("fdopen");
    return -1;
  }

  setvbuf(fsock, NULL, _IONBF, 0);
  memcpy(&(this->clientIP), &clientIP, sizeof(clientIP));

  setTimeOutPeriod(_timeOut);
  this->serviceHandler();

  fclose(fsock);
}

int Service::timeOutHandler() {}

void Service::setTimeOutPeriod(int timeOut) {
  struct timeval tv;
  tv.tv_sec = _timeOut = timeOut;
  tv.tv_usec = 0;

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}
