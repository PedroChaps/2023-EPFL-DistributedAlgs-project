//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_SENDER_H
#define DA_PROJECT_SENDER_H

#include <string>
#include <sstream>
#include "../Link/PerfectLink.h"

void sendMessageThread(int threadId, std::basic_stringstream<char> *logsBufferPtr, PerfectLink link);

class Sender {

  PerfectLink link;
  // std::vector<PerfectLink> links;

  std::string ipAddress;
  std::string port;

  std::string logsPath;
  std::stringstream *logsBufferPtr;

  // Number of messages to send
  int m;
  int processId;


public:

  Sender(std::string ip, std::string port, std::string logsPath, std::stringstream *logsBuffer, int m, int processId);
  // void sendMessageThread(int threadId);
  void sendBroadcasts();
  // Saves whatever is in `logsBuffer` to the file pointed by `logsPath`
  void saveLogs();
};


#endif //DA_PROJECT_SENDER_H
