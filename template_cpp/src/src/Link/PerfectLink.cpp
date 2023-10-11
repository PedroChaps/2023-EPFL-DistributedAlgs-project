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

  // TODO: wait for ACK of send for RETRANSMISSION_TIMEOUT milliseconds
  for (int tries = 0; tries < MAX_RETRANSMISSIONS; tries++) {

    debug("[PerfectLink] Try number " + std::to_string(tries+1) + " to send message: " + message);

    // Sends the message
    Link::send(message);

    // Set up a timer for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RETRANSMISSION_TIMEOUT * 1000; // Convert to microseconds

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
      char ackBuffer[ACK_SIZE+1];
      recvfrom(sockFd, ackBuffer, ACK_SIZE, 0, Link::getRes()->ai_addr,
               reinterpret_cast<socklen_t *>(Link::getRes()->ai_addrlen));

      debug("[PerfectLink] Received message: " + std::string(ackBuffer));
      // Check if the received message is an ACK
      if (strcmp(ackBuffer, ACK_MSG) == 0) {
        debug("[PerfectLink] it was an ACK!");
        std::cout << "ACK received. Message successfully sent." << std::endl;
        close(sockFd);
        return;
      }
    } else {
      // Timeout, retransmit the message
      std::cout << "Timeout, retransmitting message..." << std::endl;
    }
  }
}


std::string PerfectLink::receive() {

  // Receive the message
  auto receivedData = Link::receive();

  debug("[PerfectLink] Received message: " + receivedData);

  // TODO: make this concurrent by spawning a thread that ACKs continues with the interaction with the client i.e. that sends the ACK
  // TODO: Need to save the address of the client that sent the message in case it is overwritten later

  // Since the client address was stored in the `otherAddr` variable, we can use it to send the ACK

  // Create a temporary copy of sockaddr_in as sockaddr
  struct sockaddr addr;
  memcpy(&addr, &Link::getOtherAddr(), sizeof(struct sockaddr_in));

  // Call sendto with the temporary sockaddr and Link::getAddrLen()

  debug("[PerfectLink] Sending ACK...");
  sendto(Link::getUdpSocket(), ACK_MSG, ACK_SIZE, 0, &addr, sizeof(struct sockaddr_in));

/*  sendto(Link::getUdpSocket(), ackMessage, strlen(ackMessage), 0, Link::getOtherAddr(), Link::getAddrLen());*/
  std::cout << "ACK sent to client" << std::endl;
  debug("\n\n");

  return receivedData;
}