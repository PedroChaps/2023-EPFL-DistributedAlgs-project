//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_PERFECTLINK_H
#define DA_PROJECT_PERFECTLINK_H

#include "Link.h"
#include <vector>

#define MAX_RETRANSMISSIONS 5
#define RETRANSMISSION_TIMEOUT 1000


/**
 * Basically a Link, but the sending and receiving of messages is implemented as TCP.
 */
class PerfectLink : public Link {

public:

  void send(std::string message) override;

  /**
   * Additional send() version that accepts a vector of messages and sends them in burst, until every message is acknowledged to have been received.
   */
   void send(std::vector<std::string> messages);
};


#endif //DA_PROJECT_PERFECTLINK_H
