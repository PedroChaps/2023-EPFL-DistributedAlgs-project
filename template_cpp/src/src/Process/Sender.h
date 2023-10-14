//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_SENDER_H
#define DA_PROJECT_SENDER_H

#include <string>
#include <sstream>
#include "../Link/PerfectLink.h"

class Sender {

  PerfectLink link;

  std::string ipAddress;
  std::string port;

  std::string logsPath;
  std::stringstream *logsBufferPtr;

  int processId;
  // Number of messages to send
  int m;


public:

  Sender(std::string ip, std::string port, std::string logsPath, std::stringstream *logsBuffer, int m, int processId);
  void sendBroadcasts();
  // Saves whatever is in `logsBuffer` to the file pointed by `logsPath`
  void saveLogs();
};


#endif //DA_PROJECT_SENDER_H
