//
// Created by ch4ps on 04-10-2023.
//

#include "Link.h"
#include <iostream>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

/*
// Constructor for a receiver Link. Doesn't need a target IP and Port as it's the receiver
void Link::createReceiverLink(int fd, const std::string& port) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(NULL, const_cast<char*>(port.c_str()), &hints, &res) != 0) {
    perror("[Link] getaddrinfo() call failed");
    exit(1);
  }

  debug("[Link] getaddrinfo() call succeeded on the Receiver");

  if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
    perror("[Link] bind() call failed");
    exit(1);
  }

  // The socket is now bound to the port specified by the user.
  this->udpSocketFd = fd;
}

// Constructor for a sender Link. Needs a target IP and Port as it's the sender
void Link::createSenderLink(int fd, std::string ownPort, const std::string &receiverIp, const std::string& port) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if (getaddrinfo(const_cast<char*>(receiverIp.c_str()), const_cast<char*>(port.c_str()), &hints, &res) != 0) {
    perror("[Link] getaddrinfo() call failed");
    exit(1);
  }

  // The socket is now bound to the port specified by the user.
  this->udpSocketFd = fd;
}
*/
/*
// Constructor for a sender Link that calls the `createSenderLink()` method
Link::Link(int type, std::string& ownPort, const std::string& receiverIp, std::string& receiverPort) : receiverAddress(receiverIp), receiverPort(receiverPort) {

  debug("[Link] (Sender) Creating Link");
  // Create the socket
  // SOCK_DGRAM: for UDP (if using TCP, use SOCK_STREAM)
  // AF_INET: for IPv4 (if using IPv6, use AF_INET6)
  this->udpSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (this->udpSocketFd < 0) {
    perror("[Link] (Sender) socket creation failed");
    exit(1);
  }

  // Enable port reuse
  debug("[Link] (Sender) Enabling port reuse");
  int reuse = 1;
  if (setsockopt(this->udpSocketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    close(this->udpSocketFd);
    exit(1);
  }

  if (type == SENDER) {
    this->createSenderLink(this->udpSocketFd, ownPort, receiverIp, receiverPort);
  } else {
    perror("[Link] (Sender) something went wrong. Should be a Sender!");
    }

}
*/

// Constructor for a Link
Link::Link(std::string& ownPort) {

  debug("[Link] Creating Link");

  // Create the socket
  // SOCK_DGRAM: for UDP (if using TCP, use SOCK_STREAM)
  // AF_INET: for IPv4 (if using IPv6, use AF_INET6)
  this->udpSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (this->udpSocketFd < 0) {
      perror("[Link] (Receiver) socket creation failed");
      exit(1);
  }

  struct addrinfo hints;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(NULL, const_cast<char*>(ownPort.c_str()), &hints, &res) != 0) {
    perror("[Link] getaddrinfo() call failed");
    exit(1);
  }

  debug("[Link] getaddrinfo() call succeeded");
  debug("[Link] Binding socket to port " + ownPort);

  if (bind(this->udpSocketFd, res->ai_addr, res->ai_addrlen) != 0) {
    perror("[Link] bind() call failed");
    exit(1);
  }

  debug("[Link] Socket bound to port " + ownPort);
}


// Sends a message to the receiver Process on the other end of Link.
// Target process is of the type "<ip>:<port>"
void Link::send(std::string message, std::string targetProcess) {

    // Split the targetProcess into IP and Port
  std::string delimiter = ":";
  std::string targetIp = targetProcess.substr(0, targetProcess.find(delimiter));
  std::string targetPortStr = targetProcess.substr(targetProcess.find(delimiter) + 1, targetProcess.length());
  int targetPort = std::stoi(targetPortStr);

  debug("[Link] Sending message: `" + message + "` to " + targetIp + ":" + targetPortStr);

  // Create the target address in "struct sockaddr_in" form
  struct sockaddr_in targetAddr;
  memset(&targetAddr, 0, sizeof(targetAddr));
  targetAddr.sin_family = AF_INET;
  targetAddr.sin_port = htons(static_cast<uint16_t>(targetPort));
  inet_pton(AF_INET, targetIp.c_str(), &targetAddr.sin_addr);

  // Send the message
  ssize_t n = sendto(this->udpSocketFd, message.c_str(), message.length(), 0, reinterpret_cast<struct sockaddr *>(&targetAddr), sizeof(targetAddr));

  if (n < 0) {
        perror("[Link] sendto() call failed");
        exit(1);
    }
}

// Receives a message from the another Process on the other end of Link.
std::string Link::receive() {
  char buffer[BUFFER_SIZE];

  ssize_t n = recvfrom(this->udpSocketFd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&otherAddr), &addrLen);
  if (n < 0) {
    perror("[Link] recvfrom() call failed");
    exit(1);
  }

  // Create a std::string from the received data
  std::string receivedData(buffer, static_cast<size_t>(n));

  debug("[Link] Received message: `" + receivedData + "`");

  // Reset the buffer
  memset(buffer, 0, sizeof(buffer));

  return receivedData;
}

// Setters and Getters

int Link::getUdpSocket() const {
  return this->udpSocketFd;
}

struct addrinfo *Link::getRes() {
  return this->res;
}

struct sockaddr_in& Link::getOtherAddr() {
  return this->otherAddr;
}
