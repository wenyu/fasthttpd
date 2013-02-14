/*

  thread_server.cc - Implementation of Thread Server

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
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "thread_server.h"

typedef struct {
  ThreadServer *context;
  int slaveSocket;
  struct sockaddr_in clientIP;
} threadArgs;

ThreadServer::ThreadServer() : Server() {}
ThreadServer::~ThreadServer() {}

static void threadWrapper(threadArgs* args) {
  plog("%s:%d connected.", inet_ntoa(args->clientIP.sin_addr), ntohs(args->clientIP.sin_port));
  args->context->callServiceHandler(args->slaveSocket, args->clientIP);
  plog("%s:%d disconnected.", inet_ntoa(args->clientIP.sin_addr), ntohs(args->clientIP.sin_port));

  shutdown(args->slaveSocket, SHUT_RDWR);
  close(args->slaveSocket);

  delete args;

  pthread_exit(NULL);
}

int ThreadServer::run() {

  while (true) {
    struct sockaddr_in clientIP;
    socklen_t clientIPSize = sizeof( struct sockaddr_in );
    int slaveSocket = accept( _masterSocket,
			      (struct sockaddr *) &clientIP,
			      &clientIPSize
			      );

    if (slaveSocket < 0) {
      perror("accept");
      continue;
    }

    // Create arguments for thread
    threadArgs *args = new threadArgs;
    args->context = this;
    args->slaveSocket = slaveSocket;
    args->clientIP = clientIP;

    // Create thread
    pthread_t threadClient;
    pthread_create(&threadClient, NULL,
		   (void * (*)(void *))threadWrapper, 
		   (void *)args);
    pthread_detach(threadClient);
  }
  
  return 0;
}

