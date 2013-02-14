/*

  pool_server.cc - Implementation of Pool Server

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "pool_server.h"

PoolServer::PoolServer() : Server() {}
PoolServer::~PoolServer() {}

void threadWrapper(PoolServer *context) {
  context->threadRun();
}

int PoolServer::run() {
  pthread_mutex_init( &acceptLock, NULL );

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  for (int i=1; i<PARALLEL_INSTANCES; i++) {
    pthread_t thread;
    pthread_create( &thread, &attr, (void*(*)(void*))threadWrapper, this);
  }

  threadRun();

  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&acceptLock);
}

void PoolServer::threadRun() {

  struct sockaddr_in clientIP;
  socklen_t clientIPSize = sizeof( struct sockaddr_in );

  while (true) {
    
    pthread_mutex_lock( &acceptLock );
    int slaveSocket = accept( _masterSocket,
			      (struct sockaddr *) &clientIP,
			      &clientIPSize
			      );
    pthread_mutex_unlock( &acceptLock );

    if (slaveSocket < 0) {
      perror("accept");
      continue;
    }

    plog("%s:%d connected.", inet_ntoa(clientIP.sin_addr) , ntohs(clientIP.sin_port));

    callServiceHandler(slaveSocket, clientIP);

    plog("%s:%d disconnected.", inet_ntoa(clientIP.sin_addr) , ntohs(clientIP.sin_port));

    shutdown(slaveSocket, SHUT_RDWR);
    close(slaveSocket);
  }
  
  return;
}
