/*

  http_service.h - Definition of HTTP Service

  Author: Wenyu "Hearson" Zhang
  E-mail: zhangw@purdue.edu

  This file is a part of:
  Project 3 - Building a HTTP Server
  CS290 - System Programming, Fall 2010
  Purdue University

  For more information, please contact the author.

*/

#ifndef _HTTP_SERVICE_H_
#define _HTTP_SERVICE_H_

#include <string>
#include "service.h"

typedef enum {
  ERROR = -1,
  DNE = 0,
  DIRECTORY = 1,
  BINARY,
  TEXT,
  HTML,
  JPEG,
  GIF,
  PNG,
  MP4
} DocumentType;

typedef struct {
  std::string str;
  DocumentType type;
} MimeCombination;

const MimeCombination mapSuffixType[] = {

  { "GIF", GIF },
  { "HTM", HTML },
  { "HTML", HTML },
  { "JPEG", JPEG },
  { "JPG", JPEG },
  { "PNG", PNG },
  { "H", TEXT },
  { "CPP", TEXT },
  { "CC", TEXT },
  { "C", TEXT },
  { "TXT", TEXT },
  { "MP4", MP4 }

};

const MimeCombination mapTypeMime[] = {
  { "binary", BINARY },
  { "image/gif", GIF },
  { "image/jpeg", JPEG },
  { "image/png", PNG },
  { "text/html", HTML },
  { "text/plain", TEXT },
  { "video/mp4", MP4 }
};

const MimeCombination mapTypeIcon[] = {
  { "unknown.gif", BINARY },
  { "image.gif", GIF },
  { "image.gif", JPEG },
  { "image.gif", PNG },
  { "index.gif", HTML },
  { "text.gif", TEXT },
  { "image.gif", MP4 }
};

//////////////////////////////////////////////////////////////////////////////////
const int mapSuffixTypeCount = sizeof(mapSuffixType) / sizeof(mapSuffixType[0]);
const int mapTypeMimeCount = sizeof(mapTypeMime) / sizeof(mapTypeMime[0]);
const int mapTypeIconCount = sizeof(mapTypeIcon) / sizeof(mapTypeIcon[0]);

class HTTPService : public Service {
public:
  HTTPService();
  virtual ~HTTPService();
  virtual Service* clone();
  virtual int serviceHandler();
  virtual int timeOutHandler();
  virtual int initialize();

private:
  int processRequest();
  void cgiRun();
  void showStatistics();
  void showLog();
  void sendDirectory();
  void sendError301(std::string newLocation);
  void sendError404();
  void sendError501();
  void sendReply200(DocumentType docType);

private:
  static const int bufferSize = 131072;
  char buffer[bufferSize];

  std::string rootDir;
  std::string requestedDocument;
  std::string httpGetVars;
  std::string protocolVersion;
  std::string requestMethod;
  std::string postContentLength;
  bool healthy;
  bool keepAlive;

  int startPosition, endPosition, fdDoc; // The range of file is [startPosition, endPosition)
  int fdMem;

  time_t timeStart;
};

#endif // _HTTP_SERVICE_H_
