//
// Created by ch4ps on 04-10-2023.
//

#include "PerfectLink.h"
#include <iostream>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

void PerfectLink::async_retransmissor() {
  debug("[PerfectLink] (retransmissor) Starting retransmissor thread...");

  while (1) {

    debug("[PerfectLink] (retransmissor) Retransmitting unACKed messages...");

    // Iterate over the unAckedMessages and retransmit the messages
    for (auto& process : unAckedMessages) {
      for (auto& message : process.second) {
        debug("[PerfectLink] (retransmissor) Retransmitting message: `" + message.first + "` to process " + process.first);
        Link::send(message.first, process.first);
      }
    }

    usleep(1000000); // Sleep for 0.1 seconds
  }
}

PerfectLink::PerfectLink(std::string& ownPort) : Link(ownPort) {

  tRetransmissor = std::thread(&PerfectLink::async_retransmissor, this);
/*
  std::thread tRetransmissor(&PerfectLink::async_retransmissor, this);

  // Wait for them to finish
  tRetransmissor.detach();
*/

}

// Override of the `send()` method from the Link class.
// It sends a message and waits for an ACK. If it doesn't receive an ACK, it retransmits the message.
void PerfectLink::send(std::string message, std::string targetProcess) {

  // If what was sent was an ACK, just sends the message and returns
  // This way, the ACKs don't get stuck in the unAckedMessages CV (ie. the ACKs don't count towards the maximum in transit) nor should be retransmitted (ie. added to the `unAckedMessages`)
  if (message.substr(0, 3) == ACK_MSG) {
    Link::send(message, targetProcess);
    return;
  }

  // Creates block for the critical section
  {
    debug("[PerfectLink] Locking mutex to send message `" + message + "` to process " + targetProcess + "...");
    std::unique_lock<std::mutex> lock(mtx);

    // Checks how many messages are "in-flight" to the target Process and waits if there are too many
    cvs[targetProcess].wait(lock, [this, targetProcess]{
      return unAckedMessages.find(targetProcess) == unAckedMessages.end() ||
             unAckedMessages[targetProcess].size() < MAX_MSGS_IN_FLIGHT;
    });
    debug("[PerfectLink] I passed the CV (ie. currently not maximum in transit)!");

    // Sends the message
    Link::send(message, targetProcess);

    // Put the sent message in the set of messages waiting for an ACK
    if (unAckedMessages.find(targetProcess) == unAckedMessages.end()) {
      unAckedMessages[targetProcess] = std::unordered_map<std::string, int>();
    }
    unAckedMessages[targetProcess][message] = 0;
  }

  // Just returns, as the ACK will be received and treated asynchronously (or not received and the message retransmitted)
}

// Override of the `receive()` method from the Link class.
// It receives a message and sends an ACK.
std::string PerfectLink::receive() {

  std::string sourceProcess;

  // Receive the message. Also gets the address of the sender, so it can send an ACK back.
  auto receivedData = Link::receive(sourceProcess);

  debug("[PerfectLink] Received message: `" + receivedData + "`");

  // Check if the received message is an ACK by checking if it starts with "ACK"
  // ACK messages are of the format "ACK <message>", where <message> is the message that was sent (eg. "ACK MSG 1 1 2 3 4 5 6 7 8")
  if (receivedData.substr(0, 3) == ACK_MSG) {
    debug("[PerfectLink] It was an ACK!");

    // If a message was erased, notifies the sender thread
    bool erased = false;
    // Creates a block for the critical section
    {
      debug("[PerfectLink] Locking mutex to maybe erase...");
      std::unique_lock<std::mutex> lock(mtx);

      // Removes the ACK from the unAckedMessages.
      std::string message = receivedData.substr(4);
      if (unAckedMessages.find(sourceProcess) != unAckedMessages.end()) {
        if (unAckedMessages[sourceProcess].find(message) != unAckedMessages[sourceProcess].end()) {
          unAckedMessages[sourceProcess].erase(message);
          erased = true;
        }
      }
    }
    if (erased) {
      debug("[PerfectLink] A ACK was received (ie. erased) from process " + sourceProcess + ", notifying sender thread...");
      cvs[sourceProcess].notify_all();
    }

    // returns an empty string because it was an ACK, so there's nothing to deliver
    return "";
  }

  // If it's not an ACK, it's a message, so send an ACK back
  debug("[PerfectLink] Sending ACK...");
  std::string ackMessage = ACK_MSG + std::string(" ") + receivedData;
  send(ackMessage, sourceProcess);

  debug("[PerfectLink] ACK sent to client (" + sourceProcess + "): `" + ackMessage + "`");

  // Checks if the message was already received
  if (receivedPackets.find(receivedData) != receivedPackets.end()) {
    debug("[PerfectLink] Message already received, ignoring...");
    return "";
  }
  receivedPackets.insert(receivedData);

  return receivedData;
}
