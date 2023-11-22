//
// Created by ch4ps on 12/11/23.
//

#include <iostream>
#include<unistd.h>
#include "Process.h"

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}


// Constructor
/*
Process::Process(PerfectLink &link, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts) :
link(link), tReceiver(link, myPort, logsPath, logsBuffer), tSender(targetIPsAndPorts, myPort, logsPath, logsBuffer, m, nHosts, processId, link), processId(processId), targetIPsAndPorts(targetIPsAndPorts), myPort(myPort), n_messages(m) {}
*/
Process::Process(std::string myPort, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts) :
processId(processId), targetIPsAndPorts(targetIPsAndPorts), myPort(myPort), n_messages(m) {}


// Start doing stuff i.e. sending and receiving messages on both threads.
void Process::doStuff() {

/*  // Create the threads to unlock communication
  std::thread tReceiverThread(&Receiver::receiveBroadcasts, &tReceiver);
  std::thread tSenderThread(&Sender::sendBroadcasts, &tSender);

  // Wait for them to finish
  tSenderThread.join();
  tReceiverThread.join();*/

  // TODO: put this stuff in the FIFO

  std::vector<std::string> sharedVector;
  std::mutex sharedVectorMtx;

  UniformBroadcast uniformBroadcast(std::to_string(processId), targetIPsAndPorts, myPort, sharedVector, sharedVectorMtx);

  // Creates and broadcasts all the messages
  debug("[Process] Creating and sending messages...");
  for (int i = 1; i <= n_messages; i += 8) {
    // Creates a packet which is a batch of 8 messages (or until `m` is reached), and sends it
    std::string message = std::to_string(processId);
    for (int j = i; j <= i + 7 && j <= n_messages; j++) {
      message += " " + std::to_string(j);
    }
    uniformBroadcast.doUrbBroadcast(message);
  }

  debug("[Process] Done sending messages! Gonna process them now...");

  // Periodically consumes the delivered messages
  // TODO: change this, only checking if code was written well
  while (1) {
    std::vector<std::string> localCopy; // Local copy to avoid holding the lock for too long
    {
      // Acquire the lock before accessing the shared vector
      std::lock_guard<std::mutex> lock(sharedVectorMtx);
      localCopy = sharedVector;
      sharedVector.clear(); // Consume the entries
    }
    // Process the local copy of the shared vector
    std::cout << "NEW BATCH OF MESSAGES!!" << std::endl;
    for (const std::string& data : localCopy) {
      // Process data
      std::cout << "Received: " << data << std::endl;
    }
    sleep(1);
  }

}