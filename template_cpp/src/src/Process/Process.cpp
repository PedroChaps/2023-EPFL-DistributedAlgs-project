//
// Created by ch4ps on 12/11/23.
//

#include "Process.h"

// Constructor
Process::Process(std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId, std::vector<std::string> targetIPs, std::vector<std::string> targetPorts) : tReceiver(myPort, logsPath, logsBuffer), tSender(targetIPs, targetPorts, myPort, logsPath, logsBuffer, m, nHosts, processId) {}

// Start doing stuff i.e. sending and receiving messages on both threads.
void Process::doStuff() {

  // Create the threads
  std::thread tReceiverThread(&Receiver::receiveBroadcasts, &tReceiver);
  std::thread tSenderThread(&Sender::sendBroadcasts, &tSender);

  // Wait for them to finish
  tSenderThread.join();
  tReceiverThread.join();
}