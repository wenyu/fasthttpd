/*

  http_service.cc - Implementation of HTTP Service

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
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <dlfcn.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include <vector>

#include "http_service.h"

#define MAX_STRING 65536
#define CRLF "\r\n"
//#define DIRECTORY_SHOW_ICON

//////////////////////////////////////////////////////////////////
// Shared Memory

// This should take exactly 512 bytes if we have 248 bytes for these two URL.
#define STATISTICS_STRING_SIZE 248
typedef struct {
  int lock; // We must use spin lock here because it is going to be in two different processes. Mutex lock won't work in this case.
  int requests;
  int maxServiceTime;
  int minServiceTime;
  char maxSvcURL[STATISTICS_STRING_SIZE];
  char minSvcURL[STATISTICS_STRING_SIZE];
} HTTPServiceStatistics;

// This structure is to be shared between service instances
HTTPServiceStatistics *serverInfo;

#if __GNUC__ < 4
inline int __sync_lock_test_and_set(register int *ptr, register int val) {
  register int oldval = *ptr;
  *ptr = val;
  return oldval;
}
#endif

// Spin Lock Implementations
void spin_lock(int &lock) {
  while (__sync_lock_test_and_set(&lock, 1)) {
    sched_yield();
  }
}

void spin_unlock(int &lock) {
  lock = 0;
}

//////////////////////////////////////////////////////////////////
using namespace std;

HTTPService::HTTPService() : Service() {}
HTTPService::~HTTPService() {}

Service* HTTPService::clone() {
  if (_requestProcessed) return NULL;

  HTTPService *child = new HTTPService();

  child->_initialized = _initialized;
  child->_requestProcessed = _requestProcessed;
  child->_timeOut = _timeOut;
  child->sockfd = sockfd;
  child->fsock = fsock;
  child->rootDir = rootDir;
  child->keepAlive = keepAlive;
  child->healthy = healthy;
  child->fdMem = fdMem;
  child->timeStart = timeStart;

  return child;
}

int HTTPService::initialize() {
  /*
    Save current working directory, and automatically
    append "wwwroot" after it.
  */

  // Setup shard memory
  fdMem = shm_open(uusi(), O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
  if ( fdMem < 0 ) {
    perror("shm_open");
    exit(-1);
  }

  // Allocate space in that shared memory
  if (ftruncate(fdMem, sizeof(HTTPServiceStatistics)) < 0) {
    perror("shm/ftruncate");
    exit(-1);
  }

  serverInfo = (HTTPServiceStatistics *)
    mmap(NULL, sizeof(HTTPServiceStatistics), PROT_READ | PROT_WRITE, MAP_SHARED, fdMem, 0);

  if ( !serverInfo ) {
    perror("shm/mmap");
    exit(-1);
  }

  memset(serverInfo, 0, sizeof(HTTPServiceStatistics));
  serverInfo->maxServiceTime = serverInfo->minServiceTime = -1;

  ////////////////////////////////////////////////////
  char *cwd = getcwd(NULL, MAX_STRING);

  if (!cwd) {
    perror("getcwd");
    exit(1);
  }

  rootDir = cwd;
  free(cwd);

  rootDir += "/wwwroot";
  plog("Root directory is %s", rootDir.c_str());

  /*
    Check if directory exists.
  */
  DIR *rootDirEntry = opendir(rootDir.c_str());
  if (!rootDirEntry) {
    perror("opendir");
    exit(1);
  }

  closedir(rootDirEntry);

  // Default HTTP Settings;
  keepAlive = false;
  healthy = true;

  timeStart = time(0);

  Service::initialize();
}

string upcase(string src) {
  register int i;
  for (i=0; i<src.size(); i++) src[i] = toupper(src[i]);
  return src;
}

int HTTPService::processRequest() {
  if ( !fgets(buffer, bufferSize, fsock) ) {
    perror("socket/read");
    return -2;
  }

  stringstream request;
  request.str(string(buffer));
  string token, temp;

  request >> token;

  if ( token == "GET" || token == "POST" ) {
    request >> temp >> protocolVersion;
    requestMethod = token;
  } else {
    return -1;
  }

  requestedDocument = "";

  // Process %?? characters
  for (int i=0; i<temp.size(); i++) {
    if (temp[i] != '%') {
      requestedDocument += temp[i];
    } else {
      char app, hex=toupper(temp[++i]);

      if (hex >= 'A' && hex <= 'F') {
	app = hex-'A';
      } else {
	app = hex-'0';
      }

      app <<= 4;
      hex = toupper(temp[++i]);

      if (hex >= 'A' && hex <= 'F') {
	app |= hex-'A';
      } else {
	app |= hex-'0';
      }

      requestedDocument += app;
    }
  }

  plog("[ %s:%d ] requested document: %s", inet_ntoa(clientIP.sin_addr), ntohs(clientIP.sin_port), requestedDocument.c_str());

  while (fgets(buffer, bufferSize, fsock)) {
    if (buffer[0] == '\r' && buffer[1] == '\n') {
      break;
    }

    int bufferLen = strlen(buffer);
    buffer[bufferLen-1] = buffer[bufferLen-2] = '\0';

    request.clear();
    request.str(string(buffer));

    while (request.good()) {
      request >> token;
      token = upcase(token);

      if ( token == "CONNECTION:" ) {
	request >> token;

	if (upcase(token) == "KEEP-ALIVE") {
	  keepAlive = true;
	}
      } // token == "Connection"
      else if ( token == "RANGE:" ) {
	request >> token;
	if (sscanf(token.c_str(), "bytes=%d-%d", &startPosition, &endPosition) < 2) {
	  endPosition = -1;
	} else {
	  endPosition++;
	}
      } // token == "Range"
      else if ( token == "CONTENT-LENGTH:" && requestMethod == "POST" ) {
	request >> postContentLength;
      }

    } // while request.good()
  }

  return 0;
}

string cleanUpRequestPath(string src) {
  if (src[0] != '/') src = "/" + src;

  int p = 0;

  while (p < src.size()-1) {
    // Check for "?"
    if (src[p] == '?') break;

    if (src[p] == '/' && src[p+1] == '/') { // Make "//" to "/"
      src.erase(p, 1);
    } else if (src[p] == '/' && src[p+1] == '.' && src[p+2] == '/') { // Make "/./" to "/"
      src.erase(p, 2);
    } else if (src[p] == '/' && src[p+1] == '.' && src[p+2] == '.' && src[p+3] == '/') {
      int t=p-1;
      p+=3;

      if (t < 0) t=0;
      while (src[t] != '/') {
	t--;
      }

      src.erase(t, p-t);
      p = t;
    } else {
      p++;
    }
  }

  return src;
}

DocumentType getDocumentType(string path) {
  struct stat st;
  if ( stat(path.c_str(), &st) < 0 ) {
    return DNE;
  }

  if ( S_ISDIR(st.st_mode) ) {
    return DIRECTORY;
  }

  int p = path.find_last_of('.');
  if (p < path.find_last_of('/')) {
    p = (int)string::npos;
  }

  if (p == string::npos) {
    return BINARY;
  }

  string suffix = path.substr(p+1);
  for (int i=0; i<suffix.size(); i++) suffix[i] = toupper(suffix[i]);

  for (int i=0; i<mapSuffixTypeCount; i++) {
    if (suffix == mapSuffixType[i].str)
      return mapSuffixType[i].type;
  }

  return BINARY;
}

int HTTPService::serviceHandler() {
  // Attempt to read header
  string buffer;
  int err, p;

  healthy = true;
  keepAlive = true;

  struct timespec timeStart, timeEnd;
  char requestString[STATISTICS_STRING_SIZE];

  while (healthy && keepAlive) {
    keepAlive = false;
    err = 0;

    startPosition = endPosition = -1;

    err = processRequest();

    if (err == -2) break;

    // Copy request and set timer, also increase the request count
    clock_gettime(CLOCK_REALTIME, &timeStart);

    strncpy(requestString,
	    requestedDocument.substr(0, STATISTICS_STRING_SIZE-1).c_str(),
	    STATISTICS_STRING_SIZE);

    spin_lock(serverInfo->lock);
    ++(serverInfo->requests);
    spin_unlock(serverInfo->lock);

    // Start service
    keepAlive = false;

    if (err < 0 || !healthy) {
      sendError501();
      return 0;
    }

    buffer = cleanUpRequestPath(requestedDocument);

    if (buffer != requestedDocument) {
      err = 301;
      sendError301(buffer);
      continue;
    }

    httpGetVars = "";

    if ( (p=requestedDocument.find('?')) != string::npos ) {
      httpGetVars = requestedDocument.substr(p+1);
      requestedDocument = requestedDocument.substr(0, p);
    }

    string prefix = "";
    bool isCGI = false;

    if ( requestedDocument.substr(0, 9) == "/cgi-bin/") {
      isCGI = true;
    } else if ( requestedDocument.substr(0, 7) != "/icons/" ) {
      prefix = "/htdocs";
    }

    DocumentType docType = getDocumentType(rootDir + prefix + requestedDocument);

    if (docType == ERROR) {
      healthy = false;
      err = 501;
      sendError501();
    } else if (docType == DNE) {
      if (requestedDocument == "/stats") {
	showStatistics();
      } else if (requestedDocument == "/logs") {
	showLog();
      } else {
	err = 404;
	sendError404();
      }
    } else if (docType == DIRECTORY) {
      if (requestedDocument[requestedDocument.size() - 1] != '/') {
	err = 301;
	sendError301(requestedDocument + "/");
      } else {
	if (prefix != "") {
	  sendDirectory();
	} else {
	  err = 404;
	  sendError404();
	}
      }
    } else if (isCGI) {
      cgiRun();
    } else {
      fdDoc = -1;
      requestedDocument = prefix + requestedDocument;
      sendReply200(docType);
    }

    // Check service time
    clock_gettime(CLOCK_REALTIME, &timeEnd);
    int serviceTime = 1000*(int)(timeEnd.tv_sec-timeStart.tv_sec) + (int)((timeEnd.tv_nsec-timeStart.tv_nsec) / 1000000);

    spin_lock(serverInfo->lock);
    if (serverInfo->maxServiceTime < serviceTime) {
      serverInfo->maxServiceTime = serviceTime;
      strcpy(serverInfo->maxSvcURL, requestString);
    }

    if (serverInfo->minServiceTime > serviceTime || serverInfo->minServiceTime < 0) {
      serverInfo->minServiceTime = serviceTime;
      strcpy(serverInfo->minSvcURL, requestString);
    }

    spin_unlock(serverInfo->lock);
  }

  return 0;
}

int HTTPService::timeOutHandler() {
  if (fdDoc > 0) {
    close(fdDoc);
    fdDoc = -1;
  }
}

string getCurrentTimeString() {
  char buffer[64];
  time_t rawtime;
  struct tm *ptm;

  time( &rawtime );
  ptm = gmtime( &rawtime );
  strftime( buffer, 64, "%a, %d %b %Y %H:%M:%S GMT", ptm );

  return string(buffer);
}

//////////////////////////////////////////////////////
// Send Directory
int dirEntryNameCompare(const struct dirent **a, const struct dirent **b) {
  return strcmp((*b)->d_name, (*a)->d_name);
}

bool sortModifyCompare(const pair<string, string> &a, const pair<string, string> &b) {
  struct stat sta, stb;

  stat(a.second.c_str(), &sta);
  stat(b.second.c_str(), &stb);

  return sta.st_mtime < stb.st_mtime;
}

bool sortSizeCompare(const pair<string, string> &a, const pair<string, string> &b) {
  struct stat sta, stb;

  stat(a.second.c_str(), &sta);
  stat(b.second.c_str(), &stb);

  return sta.st_size < stb.st_size;
}

void HTTPService::sendDirectory() {
  struct stat st;

  if ( stat( (rootDir + "/htdocs" + requestedDocument + "index.html").c_str(), &st) == 0 ) {
    sendError301( requestedDocument + "index.html" );
    return;
  }

  string dirLocation = rootDir + "/htdocs" + requestedDocument;
  const char *request = requestedDocument.c_str();

  fprintf(fsock, "HTTP/1.0 200 OK\r\n");
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Server: Wenyu MiniHTTPD\r\n");
  fprintf(fsock, "Cache-Control: no-cache\r\n");
  fprintf(fsock, "Connection: close\r\n");
  fprintf(fsock, "Content-Type: text/html\r\n\r\n");

  // Now output directory index...
  fprintf(fsock, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n");
  fprintf(fsock, "<html><head>\r\n");
  fprintf(fsock, "<title>Index of %s</title>\r\n", request);
  fprintf(fsock, "</head><body>\r\n");
  fprintf(fsock, "<h1>Index of %s</h1>\r\n", request);

  fprintf(fsock, "<table><tr><th>&nbsp;</th><th><a href=\"./\">Name</a>&nbsp;&nbsp;&nbsp;&nbsp;</th><th><a href=\"./?M\">Last Modified</a>&nbsp;&nbsp;&nbsp;&nbsp;</th><th><a href=\"./?S\">Size</a></th></tr>\r\n");
  fprintf(fsock, "<tr><th colspan=4><hr></th></tr>\r\n");

  struct dirent **dirEntries;
  int numEntries = scandir( dirLocation.c_str(), &dirEntries, NULL, dirEntryNameCompare );

  fprintf(fsock, "<tr><td>&nbsp;</td><td><a href=\"../\">[ Parent Directory ]</a>&nbsp;&nbsp;&nbsp;&nbsp</td><td>&nbsp;&nbsp;&nbsp;&nbsp</td><td></td></tr>\r\n");

  if (numEntries < 0) {
    fprintf(fsock, "</table><h1>Something goes wrong</h1></body></html>");
    return;
  }

  vector<pair<string, string> > dirList;
  dirList.clear();

  while (numEntries--) {
    if ( strcmp(dirEntries[numEntries]->d_name, "..") && strcmp(dirEntries[numEntries]->d_name, ".") ) {
      dirList.push_back( make_pair( string(dirEntries[numEntries]->d_name),
				    dirLocation + string(dirEntries[numEntries]->d_name) ) );
    }
    free(dirEntries[numEntries]);
  }
  free(dirEntries);

  if (httpGetVars == "M") {
    sort(dirList.begin(), dirList.end(), sortModifyCompare);
  } else if (httpGetVars == "S") {
    sort(dirList.begin(), dirList.end(), sortSizeCompare);
  }

  // List all directories first...
  char timeBuff[80];
  struct tm * timeinfo;

  for (int i=0; i<dirList.size(); i++) {
    if (stat(dirList[i].second.c_str(), &st)==0 && S_ISDIR(st.st_mode)) {
      timeinfo = localtime( &st.st_mtime );
      strftime(timeBuff, 80, "%Y-%m-%d&nbsp;%H:%M:%S", timeinfo);

#ifdef DIRECTORY_SHOW_ICON
      fprintf(fsock, "<tr><td valign=\"top\"><img src=\"/icons/menu.gif\"></td><td><a href=\"%s/\">%s/</a>&nbsp;&nbsp;&nbsp;&nbsp</td><td>%s&nbsp;&nbsp;&nbsp;&nbsp</td><td>-</td></tr>\r\n", dirList[i].first.c_str(), dirList[i].first.c_str(), timeBuff);
#else
      fprintf(fsock, "<tr><td>&nbsp;</td><td><a href=\"%s/\">[ %s/ ]</a>&nbsp;&nbsp;&nbsp;&nbsp</td><td>%s&nbsp;&nbsp;&nbsp;&nbsp</td><td>-</td></tr>\r\n", dirList[i].first.c_str(), dirList[i].first.c_str(), timeBuff);
#endif
    }
  }

  // Now list files
  for (int i=0; i<dirList.size(); i++) {
    if (stat(dirList[i].second.c_str(), &st)==0 && S_ISREG(st.st_mode)) {
      timeinfo = localtime( &st.st_mtime );
      strftime(timeBuff, 80, "%Y-%m-%d&nbsp;%H:%M:%S", timeinfo);

#ifdef DIRECTORY_SHOW_ICON
      DocumentType docType = getDocumentType(dirList[i].second);
      const char *iconPath;

      for (int j=0; j<mapTypeIconCount; j++) {
	if ( mapTypeIcon[j].type == docType ) {
	  iconPath = mapTypeIcon[j].str.c_str();
	  break;
	}
      }

      fprintf(fsock, "<tr><td valign=\"top\"><img src=\"/icons/%s\"></td><td><a href=\"%s\">%s</a>&nbsp;&nbsp;&nbsp;&nbsp</td><td>%s&nbsp;&nbsp;&nbsp;&nbsp</td><td>%d</td></tr>\r\n", iconPath, dirList[i].first.c_str(), dirList[i].first.c_str(), timeBuff, (int)st.st_size);
#else
      fprintf(fsock, "<tr><td>&nbsp;</td><td><a href=\"%s\">%s</a>&nbsp;&nbsp;&nbsp;&nbsp</td><td>%s&nbsp;&nbsp;&nbsp;&nbsp</td><td>%d</td></tr>\r\n", dirList[i].first.c_str(), dirList[i].first.c_str(), timeBuff, (int)st.st_size);
#endif
    }
  }

  dirList.clear();
  fprintf(fsock, "</table></body></html>");
}

//////////////////////////////////////////////////////
// HTTP Successful
void HTTPService::sendReply200(DocumentType docType) {

  requestedDocument = rootDir + requestedDocument;

  fdDoc = open(requestedDocument.c_str(), O_RDONLY);

  if (fdDoc < 0) {
    perror("open");
    sendError404();
    return;
  }

  struct stat fileStatus;
  int contentLength;

  if (fstat(fdDoc, &fileStatus) < 0) {
    perror("fstat");
    sendError404();
    return;
  }

  startPosition = min(startPosition, (int)fileStatus.st_size);
  endPosition = min(endPosition, (int)fileStatus.st_size);

  if (startPosition <= 0) {
    startPosition = 0;
    endPosition = fileStatus.st_size;
    fprintf(fsock, "HTTP/1.0 200 OK\r\n");
  } else {
    if (endPosition < startPosition) endPosition = fileStatus.st_size;
    fprintf(fsock, "HTTP/1.0 206 Partial content\r\n");
  }

  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Server: Wenyu MiniHTTPD\r\n");
  fprintf(fsock, "Cache-Control: no-cache\r\n");
  fprintf(fsock, "Content-Range: bytes %d-%d/%d\r\n", startPosition, endPosition-1, (int)fileStatus.st_size);
  fprintf(fsock, "Content-Length: %d\r\n", contentLength=(endPosition-startPosition) );

  if (keepAlive) {
    fprintf(fsock, "Keep-Alive: timeout=%d\r\n", _timeOut);
    fputs("Connection: keep-alive\r\n", fsock);
  } else {
    fputs("Connection: close\r\n", fsock);
  }

  for (int i=0; i<mapTypeMimeCount; i++) {
    if ( docType == mapTypeMime[i].type ) {
      fprintf(fsock, "Content-Type: %s\r\n\r\n", mapTypeMime[i].str.c_str());
      break;
    }
  }

  FILE *fCachedDoc = fdopen(fdDoc, "r");
  fseek(fCachedDoc, startPosition, SEEK_SET);

  const int bufferSize = 4096; // Typical LCM of disk block size

  while (contentLength > 0 && healthy) {
    //plog("[%s]: %d bytes remaining.", requestedDocument.c_str(), contentLength);

    int batchSize = min(contentLength, bufferSize);
    int readSize = 0;
    contentLength -= batchSize;

    if ((readSize = (int)fread(buffer, 1, batchSize, fCachedDoc)) != batchSize) {
      perror("http/read");
      healthy = false;
    }

    if (fwrite(buffer, 1, readSize, fsock) != readSize) {
      perror("http/write");
      healthy = false;
    }
  }

  fclose(fCachedDoc);
  close(fdDoc);
  fdDoc = -1;
}

/////////////////////////////////////////////////////////////////
// HTTP Replies following
void HTTPService::sendError301(string newLocation) {
  static const char* errContent =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" CRLF
    "<html><head>" CRLF
    "<title>301 Moved Permanently</title>" CRLF
    "</head><body>" CRLF
    "<h1>Moved Permanently</h1>" CRLF
    "<p>The document has moved <a href=\"%s\">here</a>.</p>" CRLF
    "</body></html>" CRLF;

  fputs("HTTP/1.0 301 Moved Permanently\r\n", fsock);
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());

  if ( httpGetVars == "" ) {
    fprintf(fsock, "Location: %s\r\n", newLocation.c_str());
  } else {
    fprintf(fsock, "Location: %s?%s\r\n", newLocation.c_str(), httpGetVars.c_str());
  }

  fprintf(fsock, "Content-Length: %d\r\n", (int)(strlen(errContent)+newLocation.size()));
  if (keepAlive) {
    fprintf(fsock, "Keep-Alive: timeout=%d\r\n", _timeOut);
    fputs("Connection: keep-alive\r\n", fsock);
  } else {
    fputs("Connection: close\r\n", fsock);
  }
  fputs("Content-Type: text/html\r\n\r\n", fsock);
  fprintf(fsock, errContent, newLocation.c_str());
}

void HTTPService::sendError404() {
  static const char* errContent =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" CRLF
    "<html><head>" CRLF
    "<title>404 Not Found</title>" CRLF
    "</head><body>" CRLF
    "<h1>Not Found</h1>" CRLF
    "<p>The requested URL was not found on this server.</p>" CRLF
    "</body></html>" CRLF;

  fputs("HTTP/1.0 404 Not Found\r\n", fsock);
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Content-Length: %d\r\n", (int)strlen(errContent));
  if (keepAlive) {
    fprintf(fsock, "Keep-Alive: timeout=%d\r\n", _timeOut);
    fputs("Connection: keep-alive\r\n", fsock);
  } else {
    fputs("Connection: close\r\n", fsock);
  }
  fputs("Content-Type: text/html\r\n\r\n", fsock);
  fputs(errContent, fsock);
}

void HTTPService::sendError501() {
  static const char* errContent =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" CRLF
    "<html><head>" CRLF
    "<title>501 Method Not Implemented</title>" CRLF
    "</head><body>" CRLF
    "<h1>Method Not Implemented</h1>" CRLF
    "</body></html>" CRLF CRLF;

  healthy = false;

  fputs(errContent, fsock);
}

/////////////////////////////////////////////////////////
// Log Page
void HTTPService::showLog() {
  FILE* flog = fopen( logFilePath, "r" );

  if ( !flog ) {
    sendError404();
    return;
  }

  fprintf(fsock, "HTTP/1.0 200 OK\r\n");
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Server: Wenyu MiniHTTPD\r\n");
  fprintf(fsock, "Cache-Control: no-cache\r\n");
  if ( httpGetVars == "" ) {
    fprintf(fsock, "Location: %s\r\n", requestedDocument.c_str());
  } else {
    fprintf(fsock, "Location: %s?%s\r\n", requestedDocument.c_str(), httpGetVars.c_str());
  }
  fprintf(fsock, "Connection: close\r\n");
  fprintf(fsock, "Content-Type: text/plain\r\n\r\n");

  const int bufferSize = 4096; // Typical LCM of disk block size

  while ( healthy && !feof(flog) ) {

    int readSize = 0;

    readSize = (int)fread(buffer, 1, bufferSize, flog);

    if (fwrite(buffer, 1, readSize, fsock) != readSize) {
      perror("http/write");
      healthy = false;
    }
  }

  fclose(flog);
}

/////////////////////////////////////////////////////////
// Statistics Page
void HTTPService::showStatistics() {
  HTTPServiceStatistics status;

  spin_lock(serverInfo->lock);
  memcpy(&status, serverInfo, sizeof(HTTPServiceStatistics));
  spin_unlock(serverInfo->lock);

  fprintf(fsock, "HTTP/1.0 200 OK\r\n");
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Server: Wenyu MiniHTTPD\r\n");
  fprintf(fsock, "Cache-Control: no-cache\r\n");
  if ( httpGetVars == "" ) {
    fprintf(fsock, "Location: %s\r\n", requestedDocument.c_str());
  } else {
    fprintf(fsock, "Location: %s?%s\r\n", requestedDocument.c_str(), httpGetVars.c_str());
  }
  fprintf(fsock, "Connection: close\r\n");
  fprintf(fsock, "Content-Type: text/plain\r\n\r\n");

  fputs("Wenyu MiniHTTPD - Statistics Page" CRLF CRLF, fsock);

  int timeDiff = (int)(time(0)-timeStart);

  fprintf(fsock, "Server up time: %d day(s) %02d:%02d:%02d\r\n", timeDiff / 86400,
	  (timeDiff % 86400) / 3600, (timeDiff % 3600) / 60, timeDiff % 60);
  fprintf(fsock, "Total Requests: %d\r\n\r\n", status.requests);

  fprintf(fsock, "Maximum service time (ms): %d\r\n", status.maxServiceTime);
  fprintf(fsock, "Request for maximum service time: %s\r\n\r\n", status.maxSvcURL);

  fprintf(fsock, "Minimum service time (ms): %d\r\n", status.minServiceTime);
  fprintf(fsock, "Request for minimum service time: %s\r\n\r\n", status.minSvcURL);

  fputs("This program is written for:" CRLF, fsock);
  fputs("  CS290, Fall 2010 - System Programming / Project 3" CRLF, fsock);
  fputs("  Purdue University, West Lafayette, IN, USA" CRLF CRLF, fsock);

  fputs("For more information, please contact author:" CRLF, fsock);
  fputs("  Wenyu \"Hearson\" Zhang <zhangw@purdue.edu>" CRLF, fsock);
  fprintf(fsock, "This program was compiled at %s, %s." CRLF CRLF, __TIME__, __DATE__);
}

/////////////////////////////////////////////////////////
// CGI Implementation
void HTTPService::cgiRun() {
  const static char* cgiError =
    "Content-Type: text/html" CRLF CRLF
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" CRLF
    "<html><head>" CRLF
    "<title>CGI Script Error</title>" CRLF
    "</head><body>" CRLF
    "<h1>CGI Script Error</h1>" CRLF
    "<p>The requested CGI script cannot be executed.</p>" CRLF
    "</body></html>" CRLF;

  vector<pair<string, string> > envVars;
  envVars.clear();

#define DEFENV(A, B) envVars.push_back(make_pair(A, B))

  DEFENV("SERVER_SOFTWARE", "Wenyu MiniHTTPD");
  if ( !gethostname( buffer, bufferSize ) ) {
    DEFENV("SERVER_NAME", string(buffer));
  }

  DEFENV("SCRIPT_NAME", requestedDocument);
  DEFENV("PATH_TRANSLATED", rootDir + requestedDocument);

  DEFENV("REMOTE_ADDR", inet_ntoa(clientIP.sin_addr));
  DEFENV("SERVER_PROTOCOL", "HTTP/1.0");
  DEFENV("REQUEST_METHOD", requestMethod);

  if ( requestMethod == "POST" ) {
    DEFENV("CONTENT_TYPE", "application/x-www-form-urlencoded");
    DEFENV("CONTENT_LENGTH", postContentLength);
  }

  DEFENV("QUERY_STRING", httpGetVars);

#undef DEFENV

  fprintf(fsock, "HTTP/1.0 200 OK\r\n");
  fprintf(fsock, "Date: %s\r\n", getCurrentTimeString().c_str());
  fprintf(fsock, "Server: Wenyu MiniHTTPD\r\n");
  fprintf(fsock, "Cache-Control: no-cache\r\n");
  if ( httpGetVars == "" ) {
    fprintf(fsock, "Location: %s\r\n", requestedDocument.c_str());
  } else {
    fprintf(fsock, "Location: %s?%s\r\n", requestedDocument.c_str(), httpGetVars.c_str());
  }
  fprintf(fsock, "Connection: close\r\n");

  char * execPath = strdup((rootDir + requestedDocument).c_str());

  int childPID = fork();
  if (childPID == 0) {
    for (int i=0; i<envVars.size(); i++) {
      setenv(envVars[i].first.c_str(), envVars[i].second.c_str(), true);
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    fflush(stdin);
    dup2(sockfd, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    if ( requestedDocument.substr(requestedDocument.size()-3) == ".so" ) {
      void (*httprun)(int, char*) = NULL;
      void *lib = dlopen(execPath, RTLD_LAZY);

      if (!lib) {
	perror("http-cgi/dlopen");
	fputs(cgiError, fsock);
	fflush(stdout);
	exit(1);
      }

      httprun = (void(*)(int,char*))dlsym(lib, "httprun");
      if (!httprun) {
	perror("http-cgi/dlsym");
	fputs(cgiError, fsock);
	fflush(stdout);
	exit(1);
      }

      httprun(sockfd, strdup(httpGetVars.c_str()));
      exit(0);
    } else {
      dup2(sockfd, 1);

      vector<string> args;
      args.clear();
      int sp=0, delim;

      if (httpGetVars != "" && httpGetVars.find('=') == string::npos && httpGetVars.find('&') == string::npos ) {
	while ( (delim=httpGetVars.find('+', sp)) != string::npos ) {
	  args.push_back(httpGetVars.substr(sp, delim-sp));
	  sp = delim+1;
	}

	args.push_back(httpGetVars.substr(sp));
      }

      int argc = 2+args.size();

      char **argv = (char **)malloc(argc*sizeof(char *));
      memset(argv, 0, (argc*sizeof(char *)));

      argv[0] = execPath;
      for (int i=1; i<=args.size(); i++) {
	argv[i] = strdup(args[i-1].c_str());
      }

      args.clear();

      close(sockfd);
      execvp( execPath, argv );

      for (int i=1; i<argc-1; i++) free(argv[i]);
      free(argv);
      perror("http-cgi/execl");

      fputs(cgiError, stdout);
      fflush(stdout);

      exit(1);
    }
  } else if (childPID < 0) {
    perror("http-cgi/fork");
    healthy = false;
    fputs(cgiError, fsock);
  } else {
    plog("Executing CGI Script: %s", execPath );
    waitpid(childPID, NULL, 0);
  }

  free(execPath);
  envVars.clear();
}
