//
// Created by ch4ps on 04-10-2023.
//

#include "Receiver.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#define DEBUG 0
template <class T>
void debug(T msg) {

  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()
  ).count();

  auto time = std::chrono::system_clock::to_time_t(now);
  auto localTime = *std::localtime(&time);

  std::stringstream ss;
  ss << std::put_time(&localTime, "%F %T");
  ss << '.' << std::setfill('0') << std::setw(3) << ms % 1000; // Add milliseconds

  if (DEBUG) {
    std::cout << ss.str() << msg << std::endl;
  }
}


// Constructor for a receiver Link.
/*Receiver::Receiver(std::string port, std::string logsPath, std::stringstream *logsBuffer) : link( port), port(port), logsPath(logsPath) {
  logsBufferPtr = logsBuffer;
}*/

Receiver::Receiver(PerfectLink &link, std::string port, std::string logsPath, std::stringstream *logsBuffer) : link(link), port(port), logsPath(logsPath) {
  logsBufferPtr = logsBuffer;
}

// With the use of a PerfectLink, receives broadcasts from clients.
void Receiver::receiveBroadcasts() {

  while (1) {

    debug("[Receiver] Waiting for a message...");
    // Receive a message through the link.
    // Can be empty if the message was already received, so just ignores it.
    std::string received = link.receive();
    if (received.empty()) {
      continue;
    }

    debug("[Receiver] Got one! Extracting stuff...");
    // Extracts the process id and the sequence numbers
    // the mesage is in the format: "<process_id> <sequence_number1> <sequence_number2> ... <sequence_number8>"
    std::string processId = received.substr(0, received.find(' '));
    std::string sequenceNumbers = received.substr(received.find(' ') + 1);

    // Iterate over the sequence numbers and deliver them
    // The numbers can have multiple length, so it needs to find the spaces and extract the numbers
    std::string sequenceNumber;
    debug("[Receiver] Doing processing...");
    while (sequenceNumbers.find(' ') != std::string::npos) {
      sequenceNumber = sequenceNumbers.substr(0, sequenceNumbers.find(' '));
      sequenceNumbers = sequenceNumbers.substr(sequenceNumbers.find(' ') + 1);

      // Appends to the log variable
      (*logsBufferPtr) << "d " << processId << " " << sequenceNumber << std::endl;
    }

    // Appends the final number
    (*logsBufferPtr) << "d " << processId << " " << sequenceNumbers << std::endl;

    // saveLogs();

    // Deliver the message
    std::cout << "Received message: `" << received << "`" << std::endl;
  }
}

// After the messages are sent, writes the logs to the output file.
void Receiver::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}