/*

  main.cc - Main program of HTTP Server

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "config.h"
#include "server.h"
#include "iterative_server.h"
#include "fork_server.h"
#include "thread_server.h"
#include "pool_server.h"

#include "service.h"
#include "http_service.h"

// Server Types
const char* ServerTypeName[4] = {
  "Iterative Server",
  "Forking Server",
  "Thread Server",
  "Thread Pool Server"
};

// Basic configurations of this program
char * _programName;
int _port = DEFAULT_PORT;
ServerType _serverType = DEFAULT_SERVER_TYPE;
Server *_server = NULL;
Service *_service = NULL;

// Unique ID Gen
const char *uusi() {
  static char *str = NULL;
  if (str) return str;

  static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  static const int alphabetLen = strlen(alphabet);

  str = (char *)malloc(0x10);
  memset(str, 0, 0x10);

  srand((unsigned int)time(0));

  strcpy(str, "/myhttpd.");
  for (register int i=9; i<15; i++) {
    str[i] = alphabet[rand() % alphabetLen];
  }

  return (const char *)str;
}

void printUsage(int err = 0) {
  printf( "Usage: %s [(-i|-f|-t|-p)] [<PORT>]\n", _programName);
  printf( "Start an HTTP server and listen on <PORT> (default: %d).\n\n", DEFAULT_PORT );

  puts(   "  -i      Start an iterative server. (Default)" );
  puts(   "  -f      Start a server that creates new process for each request." );
  puts(   "  -t      Start a server that creates new thread for each request." );
  puts(   "  -p      Start a server that assigns one thread from a pool to each request.\n" );

  puts(   "  <PORT>  An integer between 0 and 65535." );
  puts(   "          This controls the port that the service will listen on. ");
  puts(   "          You must be the root to provide a port number less than 1024." );
  printf( "          If omitted, the port number will be the default value: %d.\n\n", DEFAULT_PORT );

  puts("This program is written for:");
  puts("  CS290, Fall 2010 - System Programming / Project 3");
  puts("  Purdue University, West Lafayette, IN, USA\n");

  puts("For more information, please contact author:");
  puts("  Wenyu \"Hearson\" Zhang <zhangw@purdue.edu>");
  printf("This program was compiled at %s, %s.\n\n", __TIME__, __DATE__);

  exit(err);
}

void setServerType(const ServerType type) {
  static bool reentry = false;
  if (reentry) printUsage(1);
  _serverType = type;
  reentry = true;
}

void setPort(const int port) {
  static bool reentry = false;
  if (reentry) printUsage(1);
  _port = port;
  reentry = true;
}

void processArguments(int argc, char** argv) {
  if (argc > 2) printUsage(1);

  int port;

  for (int i=0; i<argc; i++) {
    if ( !strcmp(argv[i], "-i") ) { // Iterative server
      setServerType(SERVER_TYPE_ITERATIVE);
    } else if ( !strcmp(argv[i], "-f") ) { // Forking server
      setServerType(SERVER_TYPE_FORK);
    } else if ( !strcmp(argv[i], "-t") ) { // Thread server
      setServerType(SERVER_TYPE_THREAD);
    } else if ( !strcmp(argv[i], "-p") ) { // Thread pool server
      setServerType(SERVER_TYPE_POOL);
    } else if ( sscanf(argv[i], "%d", &port) == 1 && port >= 0 && port <= 65535)  { // Port
      setPort(port);
    } else { // Error
      fprintf(stderr, "Unrecognized option: \"%s\".\n\n", argv[i]);
      printUsage(1);
    }
  }
}

void nullSignalHandler(int v) {}
void nullSigAction(int v, siginfo_t *info, void *context) {}
void blockEINTR() {
  static struct sigaction EINTRBlocker;
  memset(&EINTRBlocker, 0, sizeof(EINTRBlocker));
  EINTRBlocker.sa_sigaction = nullSigAction;
  EINTRBlocker.sa_flags = SA_RESTART | SA_NOCLDWAIT;
  sigaction(SIGCHLD, &EINTRBlocker, NULL);

  signal(SIGPIPE, nullSignalHandler);
  sigignore(SIGPIPE);
}

char logFilePath[32] = "/tmp";
FILE* flog = NULL;

int main(int argc, char** argv) {

  strcat(logFilePath, uusi());
  if ( !(flog = fopen(logFilePath, "a")) ) {
    perror("fopen");
    fprintf(stderr, "Cannot access log file %s.\n", logFilePath);
    return -1;
  }

  _programName = argv[0];
  processArguments(argc-1, argv+1);
  plog("Server type is %s.\nListening on port %d.", ServerTypeName[_serverType], _port);
  plog("Server session identifier is \"%s\".", uusi());

  // Create server object
  switch ( _serverType ) {
  case SERVER_TYPE_ITERATIVE:
    _server = new IterativeServer();
    break;
  case SERVER_TYPE_FORK:
    _server = new ForkServer();
    break;
  case SERVER_TYPE_THREAD:
    _server = new ThreadServer();
    break;
  case SERVER_TYPE_POOL:
    _server = new PoolServer();
    break;
  }

  if ( !_server ) {
    plog("Unable to create server object.");
    goto _errExit;
  }

  // Initialize service
  _service = new HTTPService();
  _service->initialize();
  _service->setTimeOutPeriod(30);

  // Initialize server and set up listening port
  _server->initialize(_port, _service, sizeof(*_service));

  if ( _server->startListen() < 0 ) {
    goto _errExit;
  }

  blockEINTR();
  _server->run();

 _errExit:
  if ( !_server ) delete _server;
  if ( !_service) delete _service;

  plog("Server terminated.");

  return 0;
}
