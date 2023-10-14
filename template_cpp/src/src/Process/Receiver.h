//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_RECEIVER_H
#define DA_PROJECT_RECEIVER_H

#include <string>
#include <sstream>
#include "../Link/PerfectLink.h"

class Receiver {

  PerfectLink link;

  std::string port;

  std::string logsPath;
  std::stringstream *logsBufferPtr;

public:

  Receiver(std::string port, std::string logsPath, std::stringstream *logsBuffer);
  void receiveBroadcasts();
};


#endif //DA_PROJECT_RECEIVER_H
