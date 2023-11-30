//
// Created by ch4ps on 12/11/23.
//

#ifndef DA_PROJECT_PROCESS_H
#define DA_PROJECT_PROCESS_H

#include "thread"
#include "Receiver.h"
#include "Sender.h"
#include "../Broadcast/UniformBroadcast.h"

/**
For delivery 2, we need (at least) two threads: one to send and one to receive (they will serve as the Sender and as the Receiver of the delivery 1 at the same time).

Otherwise:

- if we sent everything and only then received, each process would be stuck on sending, as no ACKs would be delivering
- If we received first and only then sent, no one would be sending anything

Instead of two types of processes, Sender and Receiver, we can create a single class Process, where two threads are created, one to listen and one to send (eg. TSender and TReceiver).

The TSender will behave as the Sender, just broadcasting messages in order.
The TReceiver will behave as the Receiver but with the added logic, so delivering is done casually, per process (i.e. in a FIFO fashion)
*/
class Process {

  // PerfectLink &link;
  // Receiver tReceiver;
  // Sender tSender;
  int processId;
  std::vector<std::string> targetIPsAndPorts;
  std::string myPort;
  int n_messages;

  std::string logsPath;
  std::stringstream *logsBufferPtr;

  std::mutex bufferMtx;
  std::condition_variable bufferCv;

  int round;

  void async_sendBroadcastsInRounds(UniformBroadcast &uniformBroadcast);
public:

  /**
   * Constructor.
   * @param myPort The port used to send and receive messages.
   * @param logsPath The path to the output file.
   * @param logsBuffer The buffer used to store the logs.
   * @param m The number of messages to send.
   * @param processId The id of this process, sent in the messages.
   * @param targetIPs The IP addresses of the receiver Processes.
   * @param targetPorts The ports of the receiver Processes.
   */
  // Process(PerfectLink &link, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts);
  Process(std::string myPort, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts, std::string logsPath, std::stringstream *logsBuffer);

  /**
   * Start doing stuff i.e. sending and receiving messages on both threads.
   */
  void doStuff();

  void doFIFO();

  void saveLogs();
};


#endif //DA_PROJECT_PROCESS_H
