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
        send(message.first, process.first);
      }
    }

    usleep(100000); // Sleep for 0.1 seconds
  }
}

PerfectLink::PerfectLink(std::string& ownPort) : Link(ownPort) {


  std::thread tRetransmissor(&PerfectLink::async_retransmissor, this);

  // Wait for them to finish
  tRetransmissor.detach();

}

// Override of the `send()` method from the Link class.
// It sends a message and waits for an ACK. If it doesn't receive an ACK, it retransmits the message.
void PerfectLink::send(std::string message, std::string targetProcess) {

  // Sends the message
  // TODO: change
  Link::send(message, targetProcess);

  // Put the sent message in the set of messages waiting for an ACK
  if (unAckedMessages.find(targetProcess) == unAckedMessages.end()) {
    unAckedMessages[targetProcess] = std::unordered_map<std::string, int>();
  }
  unAckedMessages[targetProcess][message] = 0;

  // Just returns, as the ACK will be received and treated asynchronously (or not received and the message retransmitted)
  return;

  /*
  // Uses the number of tries for debugging purposes and to add a naive exponential backoff
  int tries = 0;
  // Infinite loop because there is no maximum number of retransmissions
  while (tries < 10) {

    tries++;
    debug("[PerfectLink] Try number " + std::to_string(tries) + " to send message: " + message);

    // Sends the message
    Link::send(message, targetProcess);

    // TODO: temp, remove later

    // Set up a timer for receiving an ACK
    struct timeval timeout;
    // Multiplies by the number of tries to add a naive exponential backoff
    timeout.tv_usec = RETRANSMISSION_TIMEOUT * tries; // Number in microseconds (1.000.000 microseconds = 1 second)
    timeout.tv_sec = 0;

    // Reuse Socket's FD
    int sockFd = Link::getUdpSocket();

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sockFd, &readSet);

    debug("[PerfectLink] Waiting for ACK...");

    // Wait for ACK or timeout
    int ready = select(Link::getUdpSocket() + 1, &readSet, NULL, NULL, &timeout);

    if (ready > 0) {
      debug("[PerfectLink] Received a message...");
      char ackBuffer[ACK_SIZE+1] = { 0 };
      recvfrom(sockFd, ackBuffer, ACK_SIZE, 0, Link::getRes()->ai_addr,
               reinterpret_cast<socklen_t *>(Link::getRes()->ai_addrlen));

      debug("[PerfectLink] Received message: `" + std::string(ackBuffer) + "`");
      // Check if the received message is an ACK
      if (strcmp(ackBuffer, ACK_MSG) == 0) {
        debug("[PerfectLink] it was an ACK!");
        return;
      }
    } else {
      // Timeout, retransmit the message
      debug("[PerfectLink] Timeout, retransmitting message...");
    }
  sleep(1);
  }
   */

}

// Override of the `receive()` method from the Link class.
// It receives a message and sends an ACK.
std::string PerfectLink::receive() {

  std::string sourceProcess;

  // Receive the message. Also gets the address of the sender so it can send an ACK back.
  auto receivedData = Link::receive(sourceProcess);

  debug("[PerfectLink] Received message: `" + receivedData + "`");

  // Check if the received message is an ACK by checking if it starts with "ACK"
  // ACK messages are of the format "ACK <message>", where <message> is the message that was sent (eg. "ACK MSG 1 1 2 3 4 5 6 7 8")
  if (receivedData.substr(0, 3) == ACK_MSG) {
    debug("[PerfectLink] It was an ACK!");

    // Removes the ACK from the unAckedMessages.
    std::string message = receivedData.substr(4);
    if (unAckedMessages.find(sourceProcess) != unAckedMessages.end()) {
      if (unAckedMessages[sourceProcess].find(message) != unAckedMessages[sourceProcess].end()) {
        unAckedMessages[sourceProcess].erase(message);
      }
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
