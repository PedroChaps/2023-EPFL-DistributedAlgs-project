//
// Created by ch4ps on 04-10-2023.
//

#include "Receiver.h"
#include <iostream>
#include <fstream>
#include <sstream>

#define DEBUG 0
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

// Constructor for a receiver Link.
Receiver::Receiver(std::string port, std::string logsPath, std::stringstream *logsBuffer) : link(RECEIVER, port), port(port), logsPath(logsPath) {
  logsBufferPtr = logsBuffer;
}

// With the use of a PerfectLink, receives broadcasts from clients.
void Receiver::receiveBroadcasts() {

  while (1) {

    // Receive a message through the link.
    // Can be empty if the message was already received, so just ignores it.
    std::string received = link.receive();
    if (received.empty()) {
      continue;
    }

    // Extracts the process id and the sequence numbers
    // the mesage is in the format: "<process_id> <sequence_number1> <sequence_number2> ... <sequence_number8>"
    std::string processId = received.substr(0, received.find(' '));
    std::string sequenceNumbers = received.substr(received.find(' ') + 1);

    // Iterate over the sequence numbers and deliver them
    // The numbers can have multiple length, so it needs to find the spaces and extract the numbers
    std::string sequenceNumber;
    while (sequenceNumbers.find(' ') != std::string::npos) {
      sequenceNumber = sequenceNumbers.substr(0, sequenceNumbers.find(' '));
      sequenceNumbers = sequenceNumbers.substr(sequenceNumbers.find(' ') + 1);

      // Appends to the log variable
      (*logsBufferPtr) << "d " << processId << " " << sequenceNumber << std::endl;
    }

    // Appends the final number
    (*logsBufferPtr) << "d " << processId << " " << sequenceNumbers << std::endl;

    // Deliver the message
    std::cout << "Received message: `" << received << "`" << std::endl;
  }
}
