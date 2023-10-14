//
// Created by ch4ps on 04-10-2023.
//

#include "Receiver.h"
#include <iostream>
#include <fstream>
#include <sstream>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

Receiver::Receiver(std::string port, std::string logsPath, std::stringstream *logsBuffer) : link(RECEIVER, port), port(port), logsPath(logsPath) {
  logsBufferPtr = logsBuffer;
}

/**
 * With the use of a PerfectLink, receives broadcasts from clients.
 *
 * */
void Receiver::receiveBroadcasts() {

  // TODO: change it so it considers the process becoming dead
  while (1) {

    // Receive a message, if process not dead
    std::string received = link.receive();
    if (received.empty()) {
      continue;
    }

    // Extracts the process id and the sequence number
    // the mesage is in the format: "<process_id> <sequence_number>"
    std::string processId = received.substr(0, received.find(' '));
    std::string sequenceNumber = received.substr(received.find(' ') + 1);

    // Appends to the log variable
    (*logsBufferPtr) << "d " << processId << " " << sequenceNumber << std::endl;

    // Deliver the message
    std::cout << "Received message: " << received << std::endl;

  }
}
