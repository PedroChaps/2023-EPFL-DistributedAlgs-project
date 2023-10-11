//
// Created by ch4ps on 04-10-2023.
//

#include "Link.h"
#include <iostream>
#include <cstring>
#include <netdb.h>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

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

  debug("[Link] getaddrinfo() call succeeded");

  if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
    perror("[Link] bind() call failed");
    exit(1);
  }

  // The socket is now bound to the port specified by the user.
  this->udpSocketFd = fd;
}

void Link::createSenderLink(int fd, const std::string &receiverIp, const std::string& port) {
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

// Part of the implementation was taken from my previous Networking project (https://github.com/PedroChaps/RCProj-2022_2023/blob/main/server/GS.c)



// Constructor for a sender Link
Link::Link(int ownerId, int receiverId, int type, const std::string& receiverIp, std::string& receiverPort) {

  this->ownerId = ownerId;
  this->receiverId = receiverId;

  // Create the socket
  // SOCK_DGRAM: for UDP (if using TCP, use SOCK_STREAM)
  // AF_INET: for IPv4 (if using IPv6, use AF_INET6)
  this->udpSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (this->udpSocketFd < 0) {
    perror("[Link] socket creation failed");
    exit(1);
  }

  if (type == SENDER) {
    this->createSenderLink(this->udpSocketFd, receiverIp, receiverPort);
  } else {
    perror("[Link] something went wrong. Should be a Sender!");
    }

}

// Constructor for a receiver Link. Doesn't need a target IP and Port
Link::Link(int ownerId, int receiverId, int type, std::string& ownPort) {

    this->ownerId = ownerId;
    this->receiverId = receiverId;

    // Create the socket
    // SOCK_DGRAM: for UDP (if using TCP, use SOCK_STREAM)
    // AF_INET: for IPv4 (if using IPv6, use AF_INET6)
    this->udpSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (this->udpSocketFd < 0) {
        perror("[Link] socket creation failed");
        exit(1);
    }

    if (type == RECEIVER) {
        this->createReceiverLink(this->udpSocketFd, ownPort);
    } else {
      perror("[Link] something went wrong. Should be a Receiver!");
    }

}


void Link::send(std::string message) {

    ssize_t n = sendto(this->udpSocketFd, const_cast<char *>(message.c_str()), message.length() + 1, 0, res->ai_addr, res->ai_addrlen);

    if (n < 0) {
        perror("[Link] sendto() call failed");
        exit(1);
    }
}

std::string Link::receive() {
  char buffer[BUFFER_SIZE]; // Adjust the buffer size as needed

  ssize_t n = recvfrom(this->udpSocketFd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&otherAddr), &addrLen);
  if (n < 0) {
    perror("[Link] recvfrom() call failed");
    exit(1);
  }

  // Create a std::string from the received data
  std::string receivedData(buffer, static_cast<size_t>(n));

  debug("[Link] Received message: " + receivedData);

  return receivedData;
}

int Link::getOwnerId() const {
  return this->ownerId;
}

void Link::setOwnerId(int id) {
  this->ownerId = id;
}

int Link::getReceiverId() const {
  return this->receiverId;
}

void Link::setReceiverId(int id) {
  this->receiverId = id;
}

int Link::getUdpSocket() const {
  return this->udpSocketFd;
}

void Link::setUdpSocket(int socket) {
  this->udpSocketFd = socket;
}

struct addrinfo *Link::getRes() {
  return this->res;
}

struct sockaddr_in& Link::getOtherAddr() {
  return this->otherAddr;
}

socklen_t Link::getAddrLen() {
  return this->addrLen;
}

//const sockaddr_in &Link::getOwnerAddress() const {
//  return this->ownerAddress;
//}
//
//void Link::setOwnerAddress(const sockaddr_in &address) {
//  this->ownerAddress = address;
//}


