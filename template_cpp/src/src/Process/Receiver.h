//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_RECEIVER_H
#define DA_PROJECT_RECEIVER_H

#include <string>
#include "../Link/PerfectLink.h"

class Receiver {

  PerfectLink link;

  std::string port;

  std::string logsPath;

public:

  Receiver(std::string port, std::string logsPath);
  void receiveBroadcasts();
};


#endif //DA_PROJECT_RECEIVER_H
