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
#include <iomanip>
#include <chrono>
#include <ctime>

#define DEBUG 1
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
/*Sender::Sender(std::vector<std::string> targetIpsAndPorts, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId) :
  myPort(myPort), targetIpsAndPorts(targetIpsAndPorts), logsPath(logsPath), m(m), nHosts(nHosts), processId(processId), link(PerfectLink(myPort)) {

  logsBufferPtr = logsBuffer;
}*/

Sender::Sender(std::vector<std::string> targetIpsAndPorts, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId, PerfectLink &link) :
  myPort(myPort), targetIpsAndPorts(targetIpsAndPorts), logsPath(logsPath), m(m), nHosts(nHosts), processId(processId), link(link) {

  logsBufferPtr = logsBuffer;
}

// With the use of a PerfectLink, sends broadcasts to the destiny.
void Sender::sendBroadcasts() {

  for (int i = 1; i <= m; i += 8) {
    // Creates a packet which is a batch of 8 messages (or until `m` is reached), and sends it
    std::string message = std::to_string(processId);
    for (int j = i; j <= i + 7 && j <= m; j++) {
      message += " " + std::to_string(j);
    }
    for (auto target : targetIpsAndPorts) {
      link.send(message, target);
      //sleep(1);
    }

    // Appends to the log variable
    for (int j = i; j <= i + 7 && j <= m; j++) {
      (*logsBufferPtr) << "b " << j << " " << std::endl;
    }

    // Prints a confirmation
    std::cout << "Sent the message: `" << message << "`" << std::endl;
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