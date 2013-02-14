/*

config.h - Common configuration file of HTTP Server

Author: Wenyu "Hearson" Zhang
E-mail: zhangw@purdue.edu

This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

For more information, please contact the author.

*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Macros
#define plog(args...) printf(args),printf("\n"),fprintf(flog,args),fprintf(flog,"\n"),fflush(flog);
//#define plog(args...) fprintf(flog,args),fprintf(flog,"\n"),fflush(flog);

// Global Functions
const char *uusi();

// Types
typedef enum {
  SERVER_TYPE_ITERATIVE = 0,
  SERVER_TYPE_FORK = 1,
  SERVER_TYPE_THREAD = 2,
  SERVER_TYPE_POOL = 3
} ServerType;

// Constants
const int DEFAULT_PORT = 8080;
const ServerType DEFAULT_SERVER_TYPE = SERVER_TYPE_ITERATIVE;
const int REQUEST_QUEUE_SIZE = 10;

// Global Variables
extern char logFilePath[32];
extern FILE* flog;
extern char* _programName;
extern int _port;
extern ServerType _serverType;
extern const char* ServerTypeName[4];

#endif // _CONFIG_H_
