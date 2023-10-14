//
// Created by ch4ps on 04-10-2023.
//

#include "Sender.h"
#include <iostream>
#include <signal.h>
#include <fstream>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

Sender::Sender(std::string ipAddress, std::string port, std::string logsPath, std::stringstream *logsBuffer, int processId, int m) : link(SENDER, ipAddress, port), port(port), logsPath(logsPath), processId(processId), m(m) {
  logsBufferPtr = logsBuffer;
}

/**
 * With the use of a PerfectLink, sends broadcasts to the destiny.
 *
 * */
void Sender::sendBroadcasts() {

  // TODO: change it so it considers the process becoming dead
  for (int i = 1; i <= m; i++) {

    // Sends the message
    std::string message = std::to_string(processId) + " " + std::to_string(i);
    link.send(message);

    // Appends to the log variable
    (*logsBufferPtr) << "b " << i << " " << std::endl;

    // Prints a confirmation
    std::cout << "Sent message: " << message << std::endl;
  }

  saveLogs();
}

void Sender::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}