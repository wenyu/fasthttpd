/*

server.cc - General Server Interface

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#include <cstring>
#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "config.h"
#include "server.h"
#include "service.h"

Server::Server() {
  _initialized = false;
  _masterSocket = -1;
}

Server::~Server() {
  stopListen();
}

void Server::initialize(int port, Service *origService, int sizeOfService) {
  // Save parameter
  _port = port;
  _origService = origService;
  _sizeOfService = sizeOfService;
  _initialized = true;
}

int Server::startListen() {
  // Make sure user calls initialize() before listening.
  assert( _initialized );

  // Initialize sockaddr_in structure
  memset( &_serverIP, 0, sizeof( struct sockaddr_in ) );
  _serverIP.sin_family = AF_INET;
  _serverIP.sin_addr.s_addr = INADDR_ANY;
  _serverIP.sin_port = htons( (uint16_t) _port );

  // Allocate a socket
  if ( (_masterSocket = socket( PF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("socket");
    return -1;
  }

  // Set socket as to reusable.
  int socketReusable = 1;
  setsockopt( _masterSocket, SOL_SOCKET, SO_REUSEADDR, &socketReusable, sizeof(int));

  // Binding socket
  if ( bind( _masterSocket, (struct sockaddr *) &_serverIP, sizeof(struct sockaddr)) < 0 ) {
    perror("bind");
    return -1;
  }

  // Start listening
  if ( listen( _masterSocket, REQUEST_QUEUE_SIZE ) < 0 ) {
    perror("listen");
    return -1;
  }
  
  return 0;
}

void Server::stopListen() {
  if (_masterSocket >= 0) {
    if ( shutdown(_masterSocket, SHUT_RDWR) < 0 ) {
      perror("shutdown");
    }
    close( _masterSocket );
    _masterSocket = -1;
  }
}

int Server::callServiceHandler(int sockfd, struct sockaddr_in clientIP ) {
  Service* service = _origService->clone();
  
  if (!service) {
    perror("Service::clone");
    return -1;
  }

  service->processRequest( sockfd, clientIP );

  delete service;
}
