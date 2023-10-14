//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_PERFECTLINK_H
#define DA_PROJECT_PERFECTLINK_H

#include "Link.h"
#include <vector>

#define MAX_RETRANSMISSIONS 9999999
#define RETRANSMISSION_TIMEOUT 100
#define ACK_MSG "ACK"
#define ACK_SIZE 3
#include <set>

/**
 * Basically a Link, but the sending and receiving of messages is implemented as Perfect Link.
 */
class PerfectLink : public Link {

  std::set<std::string> receivedMessages;

public:

  void send(std::string message) override;
  std::string receive() override;

  PerfectLink(int type, const std::string& receiverIp, std::string& receiverPort) : Link(type, receiverIp, receiverPort){}
  PerfectLink(int type, std::string& ownPort) : Link(type, ownPort){}
};


#endif //DA_PROJECT_PERFECTLINK_H

