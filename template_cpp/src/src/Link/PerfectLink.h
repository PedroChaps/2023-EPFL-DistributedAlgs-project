//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_PERFECTLINK_H
#define DA_PROJECT_PERFECTLINK_H

#include "Link.h"
#include <vector>

#define MAX_RETRANSMISSIONS 50
#define RETRANSMISSION_TIMEOUT 1000
#define ACK_MSG "ACK"
#define ACK_SIZE 3

/**
 * Basically a Link, but the sending and receiving of messages is implemented as Perfect Link.
 */
class PerfectLink : public Link {

public:

  void send(std::string message) override;
  std::string receive() override;

  PerfectLink(int ownerId, int receiverId, int type, const std::string& receiverIp, std::string& receiverPort) : Link(ownerId, receiverId, type, receiverIp, receiverPort){}
  PerfectLink(int ownerId, int receiverId, int type, std::string& ownPort) : Link(ownerId, receiverId, type, ownPort){}
};


#endif //DA_PROJECT_PERFECTLINK_H

