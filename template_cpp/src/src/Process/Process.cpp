//
// Created by ch4ps on 12/11/23.
//

#include <iostream>
#include<unistd.h>
#include <fstream>
#include "Process.h"
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

// Constructor
Process::Process(std::string myPort, int m, int nHosts, int processId, std::vector<std::string> targetIPsAndPorts, std::string logsPath, std::stringstream *logsBuffer) :
processId(processId), targetIPsAndPorts(targetIPsAndPorts), myPort(myPort), n_messages(m), logsPath(logsPath) {

    logsBufferPtr = logsBuffer;
    round = 0;
}


void Process::async_sendBroadcastsInRounds(UniformBroadcast &uniformBroadcast) {

  // Creates and broadcasts all the messages
  debug("[Process] (sender) Creating and sending messages...");
  int i = 1;
  int lastBroadcastedRound = -1;

  while (1) {
    if (i > n_messages) {
      break;
    }
    {
      std::unique_lock<std::mutex> lock(bufferMtx);
      bufferCv.wait(lock, [this, i, lastBroadcastedRound] {
        return i <= n_messages and lastBroadcastedRound < round;
      });
      // Creates a packet which is a batch of 8 messages (or until `m` is reached), and sends it
      debug("[Process] (sender) Creating a new batch of messages with i = " + std::to_string(i));
      std::string message = std::to_string(processId);

      for (int j = i; j <= i + 7 && j <= n_messages; j++) {
        message += " " + std::to_string(j);
        (*logsBufferPtr) << "b " << j << std::endl;
      }

      uniformBroadcast.doUrbBroadcast(message);
      i += 8;
      // Periodically saves the logs
      if (i % 2400 == 0) {
        saveLogs();
      }
      lastBroadcastedRound++;
    }
  }

  debug("[Process] (sender) Done sending messages! I can relax :D");
}


void Process::doFIFO() {

  std::vector<std::string> sharedVector;
  std::mutex sharedVectorMtx;

  UniformBroadcast uniformBroadcast(std::to_string(processId), targetIPsAndPorts, myPort, sharedVector, sharedVectorMtx);

  std::thread tReceiverThread(&Process::async_sendBroadcastsInRounds, this, std::ref(uniformBroadcast));

  // Logs the sent messages
  saveLogs();

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
      // debug("[Process] Waiting for the Mutex for the shared Vector");
      std::lock_guard<std::mutex> lock(sharedVectorMtx);
      // debug("[Process] Locked the Mutex for the shared Vector");
      localCopy = sharedVector;
      // Consume the entries
      sharedVector.clear();
    }
    // debug("[Process] Unlocked the Mutex for the shared Vector");

    // Process the local copy of the shared vector
    for (const std::string& message : localCopy) {

      debug("[Receiver] About to process a new batch of " + std::to_string(localCopy.size()) + " messages...");

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

        round = std::max(round, firstPair->first / 8 + 1);
        debug("[Receiver] Round: " + std::to_string(round));

        messagesToDeliver[id].first += 8;

        std::string sequenceNumber;
        std::string sequenceNumbers = messagesToDeliver[id].second.begin()->second;

        // std::cout << "About to deliver: `" << sequenceNumbers << "` from process " << id << std::endl;

        debug("[Receiver] Doing processing...");
        {
          std::lock_guard<std::mutex> lock(bufferMtx);
          while (sequenceNumbers.find(' ') != std::string::npos) {
            sequenceNumber = sequenceNumbers.substr(0, sequenceNumbers.find(' '));
            sequenceNumbers = sequenceNumbers.substr(sequenceNumbers.find(' ') + 1);

            // Appends to the log variable
            (*logsBufferPtr) << "d " << id << " " << sequenceNumber << std::endl;
          }

          // Appends the final number
          (*logsBufferPtr) << "d " << id << " " << sequenceNumbers << std::endl;
        }
        // Every time something is delivered, notifies the other thread, so it can check if it can send more messages
        bufferCv.notify_all();
        debug("[Receiver] Done some processing. `*logsBufferPtr` now has some more content");

        messagesToDeliver[id].second.erase(firstPair);
        firstPair = (messagesToDeliver[id].second.begin());
      }

      // Checks if all messages have been delivered, by checking if the next number exceeds the limits
      if (messagesToDeliver[id].first > n_messages) {
        // If so, deletes the entry from the map
        messagesToDeliver.erase(id);
      }
    }
  }
}


//TODO: somehow use this:
//*
// * The id of the last delivered run.
// * They need to be delivered in order, so this variable helps with that.
//
//int last_delivered_run = -1;
//
//*
// * The messages that need to be delivered.
// * They are of the form `<nr1>,<nr2>,...,<nrN>`.
// * They are stored in a vector of pairs, where the first element is the run id,
// *  so it facilitates the ordering.
//
//std::vector<std::pair<int, std::string>> messages_to_deliver;
void Process::doLatticeAgreement() {

};

// After the messages are sent, writes the logs to the output file.
void Process::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}