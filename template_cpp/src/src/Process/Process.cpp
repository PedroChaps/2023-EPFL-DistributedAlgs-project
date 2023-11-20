//
// Created by ch4ps on 12/11/23.
//

#include "Process.h"

// Constructor
Process::Process(std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts) :
link(PerfectLink(myPort)), tReceiver(link, myPort, logsPath, logsBuffer), tSender(targetIPsAndPorts, myPort, logsPath, logsBuffer, m, nHosts, processId, link) {}

// Start doing stuff i.e. sending and receiving messages on both threads.
void Process::doStuff() {

  // Create the threads
  std::thread tReceiverThread(&Receiver::receiveBroadcasts, &tReceiver);
  std::thread tSenderThread(&Sender::sendBroadcasts, &tSender);

  // Wait for them to finish
  tSenderThread.join();
  tReceiverThread.join();
}