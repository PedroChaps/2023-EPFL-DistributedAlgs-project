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
#define MAX_MSGS_IN_FLIGHT 30000

#include <set>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>

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

  /**
   * Structure that keeps track of messages waiting for an ACK.
   * It's a map that looks like this:
   * {
   *    "<IP1:PORT1>": { "<message1>": <tries1>, "<message2>": <tries2>, ... },
   *    "<IP2:PORT2>": { "<message1>": <tries1>, "<message2>": <tries2>, ... },
   *    ...
   * }
   * This structure was chosen for some key reasons:
   * - It maps a target Process to the unACKed messages sent to it, so the address and port are easily accessible;
   * - It allows to easily and efficiently check if a message has already been sent to a Process;
   * However, it has the drawback of storing every message sent, which can be a lot of data (could
   * save an ID that would allow to generate the message).
   */
  std::unordered_map<std::string, std::unordered_map<std::string, int>> unAckedMessages;
  std::thread tRetransmissor;

  std::mutex mtx;
  std::unordered_map<std::string, std::condition_variable> cvs;

public:

  /**
   * Sends a message through this Perfect Link.
   * @param message The message to send.
   */
  void send(std::string message, std::string targetProcess) override;

  /**
   * Constructor for the Perfect Link.
   * @param ownPort The port of this Process.
   */
  PerfectLink(std::string& ownPort);

  /**
   * Receives a message through this Perfect Link.
   * @return The received message.
   */
  std::string receive();

  /**
   * Works in pair with the `unAckedMessages` structure.
   * Routinely iterates over the structure and retransmits the messages, as they haven't been ACKed.
   * Will be called asynchronously, so it must be called in a separate thread.
   */
  void async_retransmissor();

  /**
   * Destructor. Was necessary because of some cryptic error.
   */
  virtual ~PerfectLink() = default;
};


#endif //DA_PROJECT_PERFECTLINK_H

