/*

  iterative_server.cc - Implementation of Iterative Server

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

#include "iterative_server.h"

IterativeServer::IterativeServer() : Server() {}
IterativeServer::~IterativeServer() {}

int IterativeServer::run() {

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

    plog("%s:%d connected.", inet_ntoa(clientIP.sin_addr) , ntohs(clientIP.sin_port));

    callServiceHandler(slaveSocket, clientIP);

    plog("%s:%d disconnected.", inet_ntoa(clientIP.sin_addr) , ntohs(clientIP.sin_port));

    shutdown(slaveSocket, SHUT_RDWR);
    close(slaveSocket);
  }
  
  return 0;
}
