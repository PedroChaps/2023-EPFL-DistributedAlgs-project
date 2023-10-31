//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_LINK_H
#define DA_PROJECT_LINK_H


#include <netinet/in.h>
#include <string>

#define RECEIVER 0
#define SENDER 1

#define BUFFER_SIZE 65536 // 64KiB, according to the project description


// Part of the implementation was taken from my previous Networking project (https://github.com/PedroChaps/RCProj-2022_2023/blob/main/server/GS.c)
/**
 * Representation of a Link that will enable the connection of two Processes, either to send or to receive messages.

 * When a Process wants to send a message to another Process, it will use a Link to easily send it.
 * To do so, the Process creates a Link and calls the `send()` method with the message.
 *
 * Conversely, when a Process wants to receive a message from another Process, it will use a Link to easily receive it.
 *
 * It is built on top of UDP sockets, so it is unreliable.
 *
 * Note: This class shouldn't be used, as it's unreliable - only one message is sent and, since it's using UDP,
 *  is not guaranteed to reach the destination.
 * Please consider using the PerfectLink class instead.
 */
class Link {

  /**
   * The socket used to send messages through this Link.
   */
  int udpSocketFd;

  /**
   * The address of the Process on the other end of the Link.
   */
  struct sockaddr_in otherAddr;
  socklen_t addrLen = sizeof(otherAddr);

  /**
   * (If sending) The address & port of the receiver Process.
   */
  std::string receiverAddress;
  std::string receiverPort;

  /**
   * Extra information about the receiver Process, used to save more information about the sender/receiver.
   */
  struct addrinfo *res;

  /**
   * methods to construct the Link, based on the type of the Link.
   */
  void createReceiverLink(int fd, const std::string& port);

  void createSenderLink(int fd, const std::string& receiverIp, const std::string& port);

public:

  /**
   * Creates a Link that will be used to send messages.
   */
  Link(int type, const std::string& receiverIp, std::string& receiverPort);
  Link(int type, std::string& ownPort);

  /**
   * Sends a message to the receiver Process.
   * @param message The message to send.
   */
  virtual void send(std::string message);

  /**
   * Receives a message from the sender Process.
   * @return The received message.
   */
  virtual std::string receive();

  /**
   * Setters and Getters
   * */
  int getUdpSocket() const;
  struct addrinfo *getRes();
  struct sockaddr_in& getOtherAddr();
};


#endif //DA_PROJECT_LINK_H
