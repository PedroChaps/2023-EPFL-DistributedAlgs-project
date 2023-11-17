//
// Created by ch4ps on 04-10-2023.
//

#include "Sender.h"
#include <iostream>
#include <signal.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include<unistd.h>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

// Constructor for a receiver Link.
Sender::Sender(std::vector<std::string> targetIps, std::vector<std::string> targetPorts, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId) : myPort(myPort), targetIps(targetIps), targetPorts(targetPorts), logsPath(logsPath), m(m), nHosts(nHosts), processId(processId) {

  logsBufferPtr = logsBuffer;
  links = std::vector<PerfectLink>();

  // Creates a PerfectLink for each other process
  for (unsigned long i = 0; i < static_cast<unsigned long>(nHosts); i++) {
    links.push_back(PerfectLink(SENDER, myPort, targetIps[i], targetPorts[i]));
  }
}

// With the use of a PerfectLink, sends broadcasts to the destiny.
void Sender::sendBroadcasts() {

  for (int i = 1; i <= m; i += 8) {

    // Creates a packet which is a batch of 8 messages (or until `m` is reached), and sends it
    std::string message = std::to_string(processId);
    for (int j = i; j <= i + 7 && j <= m; j++) {
      message += " " + std::to_string(j);
    }
    for (auto link : links) {
      link.send(message);
    }

    // Appends to the log variable
    for (int j = i; j <= i + 7 && j <= m; j++) {
      (*logsBufferPtr) << "b " << j << " " << std::endl;
    }

    // Prints a confirmation
    std::cout << "Sent the message (that got ACKed): `" << message << "`" << std::endl;
  }

  saveLogs();
}

// After the messages are sent, writes the logs to the output file.
void Sender::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}