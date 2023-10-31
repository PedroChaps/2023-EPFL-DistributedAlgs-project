//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_PERFECTLINK_H
#define DA_PROJECT_PERFECTLINK_H

#include "Link.h"
#include <vector>

#define RETRANSMISSION_TIMEOUT 40000 // (1.000.000 microseconds = 1 second; 40.000 microseconds = 0.04 seconds)
#define ACK_MSG "ACK"
#define ACK_SIZE 3

#include <set>

// Part of the implementation was taken from my previous Networking project (https://github.com/PedroChaps/RCProj-2022_2023/blob/main/server/GS.c)
/**
 * Basically a Link, but the sending and receiving of messages is implemented as Perfect Link i.e. formally the following is guaranteed:
 * - If a message is sent, it will eventually be received by the other Process.
 * - If a message is received, it was previously sent by the other Process.
 * - If a message is received more than once, it was sent more than once by the other Process.
 *
 * In other words, it is a reliable Link and the following happens:
 * - For each message sent by the Sender, the Receiver will reply with an Acknowledgement (ACK).
 * - If after a timeout the Sender doesn't receive an ACK, it will resend the message.
 * - If the Receiver receives a message that it has already received, it will reply with an ACK and not process / deliver it.
 */
class PerfectLink : public Link {

  /**
   * Set that keeps track of messages the have already been received.
   */
  std::set<std::string> receivedPackets;

public:

  /**
   * Sends a message through this Perfect Link.
   * @param message The message to send.
   */
  void send(std::string message) override;

  /**
   * Constructors, for the Sender and the Receiver.
   * @param type The type of the Link (SENDER or RECEIVER).
   * @param receiverIp The IP of the receiver Process.
   * @param receiverPort The port of the receiver Process.
   */
  PerfectLink(int type, const std::string& receiverIp, std::string& receiverPort) : Link(type, receiverIp, receiverPort){}
  PerfectLink(int type, std::string& ownPort) : Link(type, ownPort){}

  /**
   * Receives a message through this Perfect Link.
   * @return The received message.
   */
  std::string receive() override;


  /**
   * Destructor. Was necessary because of some cryptic error.
   */
  virtual ~PerfectLink() = default;
};


#endif //DA_PROJECT_PERFECTLINK_H

