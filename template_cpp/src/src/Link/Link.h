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

/**
 * Representation of a Link that will enable the connection of two Processes, either to send or to receive messages.

 * When a Process wants to send a message to another Process, it will use a Link to easily send it.
 * To do so, the Process creates a Link and calls the send method with the message.
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
   * The id of the Process that creates the Link.
   */
  int ownerId;

  std::string ownerAddress;

  struct sockaddr_in otherAddr;
  socklen_t addrLen = sizeof(otherAddr);

  /**
   * (If sending) The id of the receiver Process.
   */
  int receiverId;

  std::string receiverAddress;
  std::string receiverPort;

  /**
   * The socket used to send messages through this Link.
   */
  int udpSocketFd;

  /**
   * The address of the Process that sends messages through this Link.
   */
//  struct sockaddr_in senderAddress;

  struct addrinfo *res;
  /**
   * methods to construct the Link, based on the type of the Link.
   */
  void createReceiverLink(int fd, const std::string& port);

  void createSenderLink(int fd, const std::string& receiverIp, const std::string& port);

public:

  /**
   * Creates a Link that will be used to send messages.
   *
   * TODO: update docs
   */
  Link(int type, const std::string& receiverIp, std::string& receiverPort);
  Link(int type, std::string& ownPort);

  /**
   * Sends a message to the receiver Process.
   */
  virtual void send(std::string message);

  /**
   * Receives a message from the sender Process.
   */
  virtual std::string receive();

  /**
   * Setters and Getters
   * */
  int getOwnerId() const;
  void setOwnerId(int ownerId);
  int getReceiverId() const;
  std::string getReceiverAddress();
  std::string getReceiverPort();
  void setReceiverId(int receiverId);
  int getUdpSocket() const;
  void setUdpSocket(int socket);
  const sockaddr_in &getOwnerAddress() const;
  void setOwnerAddress(const sockaddr_in &ownerAddress);
  struct addrinfo *getRes();
  struct sockaddr_in& getOtherAddr();
  socklen_t getAddrLen();

//  const sockaddr_in &getSenderAddress() const;
//  int setSenderAddress(const sockaddr_in &senderAddress);

};


#endif //DA_PROJECT_LINK_H
