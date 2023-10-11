//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_SENDER_H
#define DA_PROJECT_SENDER_H

#include <string>
#include "../Link/PerfectLink.h"

class Sender {

  PerfectLink link;

  std::string ipAddress;
  std::string port;

  std::string logsPath;

  int processId;
  // Number of messages to send
  int m;


public:

  Sender(std::string ip, std::string port, std::string logsPath, int m, int processId);
  void sendBroadcasts();
};


#endif //DA_PROJECT_SENDER_H
