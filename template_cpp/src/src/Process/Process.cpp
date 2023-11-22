//
// Created by ch4ps on 12/11/23.
//

#include <iostream>
#include<unistd.h>
#include <fstream>
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
Process::Process(std::string myPort, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts, std::string logsPath, std::stringstream *logsBuffer) :
processId(processId), targetIPsAndPorts(targetIPsAndPorts), myPort(myPort), n_messages(m), logsPath(logsPath) {

    logsBufferPtr = logsBuffer;
}


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


void Process::doFIFO() {


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

    for (int j = i; j <= i + 7 && j <= n_messages; j++) {
      (*logsBufferPtr) << "b " << j << std::endl;
    }
  }

  // Logs the sent messages
  saveLogs();

  debug("[Process] Done sending messages! Going to process them now...");

  // Creates an ugly data structure to store the messages while they are not in order.
  // I hope the following explanation is good, otherwise I am sorry if you are trying to understand it and cant :(
  /**

    Creates a structure which purpose is to store the messages while they cannot be delivered in order (eg. some prior messages are still in-flight because of delays).
    It maps the id of the sender to a list of not-yet-delivered messages.
    Some additional information is also stored, such as the next "sequence number" to deliver and the first "sequence number" of each message, to make the algorithm simpler.
    The set is used to keep the messages sorted by the first "sequence number" of each message. To do this, it uses a custom comparator, which compares the first entry of the saved pairs.

    The structure is of the form:
    ```
    {
      <id1>: (<nextMsgToDeliver1>, { (<firstElement1>, <msg1>), (<firstElement2>, <msg2>), ... } ),
      <id2>: (<nextMsgToDeliver1>, { (<firstElement1>, <msg1>), (<firstElement2>, <msg2>), ... } ),
        ...
    }
   ```

   An example is the following:
   ```
   {
      "1": (9, { (17, "17 18 19 20 21 22 23 24"), (25, "25 26 27 28 29 30 31 32") } ),
      "2": (17, { (25, "25 26 27 28 29 30 31 32") } ),
      "3": (1, { } ),
    }
   ```
   */

  // Comparator used to sort the messages by the first element of the pair
  struct custom_compare {
    // Returns true if a < b
    bool operator() (const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) const {
      return a.first < b.first;
    }
  };

  std::unordered_map<
          std::string, std::pair<
                  int, std::set<
                          std::pair<int, std::string>,
                          custom_compare
                          >
                  >
  > messagesToDeliver;


  // Periodically consumes the delivered messages
  while (1) {
    // Local copy to avoid holding the lock for too long
    std::vector<std::string> localCopy;
    {
      // Acquire the lock before accessing the shared vector
      std::lock_guard<std::mutex> lock(sharedVectorMtx);
      localCopy = sharedVector;
      // Consume the entries
      sharedVector.clear();
    }

    // Process the local copy of the shared vector
    debug("[Receiver] About to process a new batch of " + std::to_string(localCopy.size()) + " messages...");
    for (const std::string& message : localCopy) {

      // Split the message. It's of the form "<id> <msg1> <msg2> ... <msg8>"
      std::string id = message.substr(0, message.find(' '));
      std::string msgs = message.substr(message.find(' ') + 1);
      int firstMsg = stoi(msgs.substr(0, msgs.find(' ')));

      // Checks if id exists in the map. If not, creates it.
      if (messagesToDeliver.find(id) == messagesToDeliver.end()) {
        messagesToDeliver[id] = std::make_pair(1, std::set<std::pair<int, std::string>, custom_compare>(custom_compare{}));
      }

      // Adds the message to the set (since it's sorted, it will be in the correct position for the next step)
      messagesToDeliver[id].second.insert(std::make_pair(firstMsg, msgs));

      // Iteratevely goes over the messages and delivers everything it can
      auto firstPair = (messagesToDeliver[id].second.begin());
      while (messagesToDeliver[id].first == firstPair->first) {
        // If the message can be delivered:
        // - Delivers it
        // - Increases the next expected sequence number to deliver
        // - Erases the message from the set
        // - Updates the firstPair to the newer first element

        messagesToDeliver[id].first += 8;
        // TODO: change delivery to writing to a file

        std::string sequenceNumber;
        std::string sequenceNumbers = messagesToDeliver[id].second.begin()->second;

        std::cout << "About to deliver: " << sequenceNumbers << " from process " << id << std::endl;

        debug("[Receiver] Doing processing...");
        while (sequenceNumbers.find(' ') != std::string::npos) {
          sequenceNumber = sequenceNumbers.substr(0, sequenceNumbers.find(' '));
          sequenceNumbers = sequenceNumbers.substr(sequenceNumbers.find(' ') + 1);

          // Appends to the log variable
          (*logsBufferPtr) << "d " << id << " " << sequenceNumber << std::endl;
        }

        // Appends the final number
        (*logsBufferPtr) << "d " << id << " " << sequenceNumbers << std::endl;

        messagesToDeliver[id].second.erase(firstPair);
        firstPair = (messagesToDeliver[id].second.begin());
      }

      // Checks if all messages have been delivered, by checking if the next number exceeds the limits
      if (messagesToDeliver[id].first > n_messages) {
        // If so, deletes the entry from the map
        messagesToDeliver.erase(id);
      }
    }
    sleep(1);
  }
}

// After the messages are sent, writes the logs to the output file.
void Process::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}