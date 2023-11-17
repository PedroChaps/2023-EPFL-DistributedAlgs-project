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

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

// Override of the `send()` method from the Link class.
// It sends a message and waits for an ACK. If it doesn't receive an ACK, it retransmits the message.
void PerfectLink::send(std::string message) {

  // Uses the number of tries for debugging purposes and to add a naive exponential backoff
  int tries = 0;
  // Infinite loop because there is no maximum number of retransmissions
  while (1) {

    tries++;
    debug("[PerfectLink] Try number " + std::to_string(tries) + " to send message: " + message);

    // Sends the message
    Link::send(message);

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

      debug("[PerfectLink] Received message: " + std::string(ackBuffer));
      // Check if the received message is an ACK
      if (strcmp(ackBuffer, ACK_MSG) == 0) {
        debug("[PerfectLink] it was an ACK!");
        return;
      }
    } else {
      // Timeout, retransmit the message
      debug("[PerfectLink] Timeout, retransmitting message...");
    }
  }
}

// Override of the `receive()` method from the Link class.
// It receives a message and sends an ACK.
std::string PerfectLink::receive() {

  // Receive the message
  auto receivedData = Link::receive();
  debug("[PerfectLink] Received message: `" + receivedData + "`");

  // Create a temporary copy of sockaddr_in as sockaddr because of the sendto() type of arguments
  struct sockaddr_in addrIPv4;
  memcpy(&addrIPv4, &Link::getOtherAddr(), sizeof(struct sockaddr_in));

  // Extract IP address and port from sockaddr_in
  char ipStr[INET_ADDRSTRLEN];
  int port = ntohs(addrIPv4.sin_port);

  inet_ntop(AF_INET, &(addrIPv4.sin_addr), ipStr, INET_ADDRSTRLEN);

  debug("[PerfectLink] Sending ACK...");

  // Use the extracted IP address and port in your message
  sendto(Link::getUdpSocket(), ACK_MSG, ACK_SIZE, 0, reinterpret_cast<struct sockaddr*>(&addrIPv4), sizeof(struct sockaddr_in));
  std::string ackMessage = "[PerfectLink] ACK sent to client (" + std::string(ipStr) + ":" + std::to_string(port) + ")";
  debug(ackMessage);

  // Checks if the message was already received
  if (receivedPackets.find(receivedData) != receivedPackets.end()) {
    debug("[PerfectLink] Message already received, ignoring...");
    return "";
  }
  receivedPackets.insert(receivedData);

  return receivedData;
}
