//
// Created by ch4ps on 04-10-2023.
//

#include "PerfectLink.h"
#include <iostream>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}


void PerfectLink::send(std::string message) {

  int tries = 0;
  while (1) {
    tries++;

    debug("[PerfectLink] Try number " + std::to_string(tries) + " to send message: " + message);

    // Sends the message
    Link::send(message);

    // Set up a timer for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;
    // Multiplies by the number of tries to add a naive exponential backoff
    timeout.tv_usec = RETRANSMISSION_TIMEOUT * tries; // Number in microseconds (1.000.000 microseconds = 1 second)

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


/*
std::string PerfectLink::receive() {

  // Receive the message
  auto receivedData = Link::receive();
  debug("[PerfectLink] Received message: " + receivedData);


  // TODO: make this concurrent by spawning a thread that ACKs continues with the interaction with the client i.e. that sends the ACK
  // TODO: Need to save the address of the client that sent the message in case it is overwritten later

  // Since the client address was stored in the `otherAddr` variable, we can use it to send the ACK

  // Create a temporary copy of sockaddr_in as sockaddr because of the sendto() type of arguments
  struct sockaddr addr;
  memcpy(&addr, &Link::getOtherAddr(), sizeof(struct sockaddr_in));

  debug("[PerfectLink] Sending ACK...");

  sendto(Link::getUdpSocket(), ACK_MSG, ACK_SIZE, 0, &addr, sizeof(struct sockaddr_in));
  debug("[PerfectLink] ACK sent to client (" + Link::getReceiverAddress() + ":" + Link::getReceiverPort() + ")");

  // Checks if the message was already received
  if (receivedMessages.find(receivedData) != receivedMessages.end()) {
    debug("[PerfectLink] Message already received, ignoring...");
    return "";
  }
  receivedMessages.insert(receivedData);

  return receivedData;
}*/

std::string PerfectLink::receive() {
  // Receive the message
  auto receivedData = Link::receive();
  debug("[PerfectLink] Received message: " + receivedData);

  // Create a temporary copy of sockaddr_in as sockaddr because of the sendto() type of arguments
  struct sockaddr_in addrIPv4;
  memcpy(&addrIPv4, &Link::getOtherAddr(), sizeof(struct sockaddr_in));

  // Extract IP address and port from sockaddr_in
  char ipStr[INET_ADDRSTRLEN];
  int port = ntohs(addrIPv4.sin_port);

  inet_ntop(AF_INET, &(addrIPv4.sin_addr), ipStr, INET_ADDRSTRLEN);

  debug("[PerfectLink] Sending ACK...");

  // Use the extracted IP address and port in your message
  std::string ackMessage = "[PerfectLink] ACK sent to client (" + std::string(ipStr) + ":" + std::to_string(port) + ")";
  sendto(Link::getUdpSocket(), ACK_MSG, ACK_SIZE, 0, reinterpret_cast<struct sockaddr*>(&addrIPv4), sizeof(struct sockaddr_in));
  debug(ackMessage);

  // Checks if the message was already received
  if (receivedMessages.find(receivedData) != receivedMessages.end()) {
    debug("[PerfectLink] Message already received, ignoring...");
    return "";
  }
  receivedMessages.insert(receivedData);

  return receivedData;
}
