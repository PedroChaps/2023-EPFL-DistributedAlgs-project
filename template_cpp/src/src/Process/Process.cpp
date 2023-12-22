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
#include <algorithm>
#include <unistd.h>

#define DELTA_RUNS 3

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


bool comparePairs(const std::pair<int, std::string> &a, const std::pair<int, std::string> &b);

// Constructor
Process::Process(std::string myPort, int p, int nHosts, int processId, std::unordered_map<std::string,std::string> idToIPAndPort, std::string configPath, std::string logsPath, std::stringstream *logsBuffer) :
        processId(processId), idToIPAndPort(idToIPAndPort), myPort(myPort), n_proposals(p), configPath(configPath), logsPath(logsPath) {

    logsBufferPtr = logsBuffer;
    round = 0;
}

// FIXME: just sending the messages one by one, without using the batches
void Process::async_enqueueMessagesToBroadcast(LatticeAgreement &latticeAgreement) {

  // This function will read the config file to get the proposed sets

  std::ifstream file(configPath);
  if (not file.is_open()) {
    std::cerr << "Unable to open file: " << configPath << std::endl;
    exit(1);
  }

  std::string line;
  // Ignores the first line (the one with some parameters), as it was already read in the constructor
  std::getline(file, line);
  line = "";

//
//     // TODO: remove (bcz it's read in Process)
//     while (std::getline(file, line)) {
//       values.inputSets.push_back(line);
//     }
//
//     file.close();
//

  std::vector<std::string> inputSets;

  // Creates and broadcasts all the messages
  debug("[Process] (sender) Creating and sending messages...");
  int i = 0;
  int lastBroadcastedRun = -1;

  while (1) {
    if (i >= n_proposals) {
      break;
    }
    {
      std::unique_lock<std::mutex> lock(bufferMtx);
      bufferCv.wait(lock, [this, i, lastBroadcastedRun] {
        // Waits for a maximum delta of DELTA_RUNS runs (ie. only DELTA_RUNS runs can be in-flight at the same time)
        return i <= n_proposals and lastBroadcastedRun - lastDeliveredRun <= DELTA_RUNS;
      });

      // Reads the next line
      std::getline(file, line);
      if (line.empty()) {
        // If the line is empty, it means we reached the end of the file
        break;
      }

      debug("[Process] (sender) Starting run " + std::to_string(i) + " with input set `" + line + "`");
      latticeAgreement.startRun(i, line);

      // Periodically saves the logs
      if (i % 240 == 0) {
        saveLogs();
      }
      i++;
      lastBroadcastedRun++;
    }
  }

  debug("[Process] (sender) Done sending messages! I can relax :D");
}


//void Process::doFIFO() {
//
//  std::vector<std::string> sharedVector;
//  std::mutex sharedVectorMtx;
//
//  UniformBroadcast uniformBroadcast(std::to_string(processId), idToIPAndPort, myPort, sharedVector, sharedVectorMtx);
//
//  std::thread tSenderThread(&Process::async_enqueueMessagesToBroadcast, this, std::ref(uniformBroadcast));
//
//  // Logs the sent messages
//  saveLogs();
//
//  // Creates an ugly data structure to store the messages while they are not in order.
//  // I hope the following explanation is good, otherwise I am sorry if you are trying to understand it and cant :(
//  /**
//
//    Creates a structure which purpose is to store the messages while they cannot be delivered in order (eg. some prior messages are still in-flight because of delays).
//    It maps the id of the sender to a list of not-yet-delivered messages.
//    Some additional information is also stored, such as the next "sequence number" to deliver and the first "sequence number" of each message, to make the algorithm simpler.
//    The set is used to keep the messages sorted by the first "sequence number" of each message. To do this, it uses a custom comparator, which compares the first entry of the saved pairs.
//
//    The structure is of the form:
//    ```
//    {
//      <id1>: (<nextMsgToDeliver1>, { (<firstElement1>, <msg1>), (<firstElement2>, <msg2>), ... } ),
//      <id2>: (<nextMsgToDeliver1>, { (<firstElement1>, <msg1>), (<firstElement2>, <msg2>), ... } ),
//        ...
//    }
//   ```
//
//   An example is the following:
//   ```
//   {
//      "1": (9, { (17, "17 18 19 20 21 22 23 24"), (25, "25 26 27 28 29 30 31 32") } ),
//      "2": (17, { (25, "25 26 27 28 29 30 31 32") } ),
//      "3": (1, { } ),
//    }
//   ```
//   */
//
//  // Comparator used to sort the messages by the first element of the pair
//  struct custom_compare {
//    // Returns true if a < b
//    bool operator() (const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) const {
//      return a.first < b.first;
//    }
//  };
//
//  std::unordered_map<
//          std::string, std::pair<
//                  int, std::set<
//                          std::pair<int, std::string>,
//                          custom_compare
//                          >
//                  >
//  > messagesToDeliver;
//
//
//  // Periodically consumes the delivered messages
//  while (1) {
//    // Local copy to avoid holding the lock for too long
//    std::vector<std::string> localCopy;
//    {
//      // Acquire the lock before accessing the shared vector
//      // debug("[Process] Waiting for the Mutex for the shared Vector");
//      std::lock_guard<std::mutex> lock(sharedVectorMtx);
//      // debug("[Process] Locked the Mutex for the shared Vector");
//      localCopy = sharedVector;
//      // Consume the entries
//      sharedVector.clear();
//    }
//    // debug("[Process] Unlocked the Mutex for the shared Vector");
//
//    // Process the local copy of the shared vector
//    for (const std::string& message : localCopy) {
//
//      debug("[Receiver] About to process a new batch of " + std::to_string(localCopy.size()) + " messages...");
//
//      // Split the message. It's of the form "<id> <msg1> <msg2> ... <msg8>"
//      std::string id = message.substr(0, message.find(' '));
//      std::string msgs = message.substr(message.find(' ') + 1);
//      int firstMsg = stoi(msgs.substr(0, msgs.find(' ')));
//
//      // Checks if id exists in the map. If not, creates it.
//      if (messagesToDeliver.find(id) == messagesToDeliver.end()) {
//        messagesToDeliver[id] = std::make_pair(1, std::set<std::pair<int, std::string>, custom_compare>(custom_compare{}));
//      }
//
//      // Adds the message to the set (since it's sorted, it will be in the correct position for the next step)
//      messagesToDeliver[id].second.insert(std::make_pair(firstMsg, msgs));
//
//      // Iteratevely goes over the messages and delivers everything it can
//      auto firstPair = (messagesToDeliver[id].second.begin());
//      while (messagesToDeliver[id].first == firstPair->first) {
//        // If the message can be delivered:
//        // - Delivers it
//        // - Increases the next expected sequence number to deliver
//        // - Erases the message from the set
//        // - Updates the firstPair to the newer first element
//
//        round = std::max(round, firstPair->first / 8 + 1);
//        debug("[Receiver] Round: " + std::to_string(round));
//
//        messagesToDeliver[id].first += 8;
//
//        std::string sequenceNumber;
//        std::string sequenceNumbers = messagesToDeliver[id].second.begin()->second;
//
//        // std::cout << "About to deliver: `" << sequenceNumbers << "` from process " << id << std::endl;
//
//        debug("[Receiver] Doing processing...");
//        {
//          std::lock_guard<std::mutex> lock(bufferMtx);
//          while (sequenceNumbers.find(' ') != std::string::npos) {
//            sequenceNumber = sequenceNumbers.substr(0, sequenceNumbers.find(' '));
//            sequenceNumbers = sequenceNumbers.substr(sequenceNumbers.find(' ') + 1);
//
//            // Appends to the log variable
//            (*logsBufferPtr) << "d " << id << " " << sequenceNumber << std::endl;
//          }
//
//          // Appends the final number
//          (*logsBufferPtr) << "d " << id << " " << sequenceNumbers << std::endl;
//        }
//        // Every time something is delivered, notifies the other thread, so it can check if it can send more messages
//        bufferCv.notify_all();
//        debug("[Receiver] Done some processing. `*logsBufferPtr` now has some more content");
//
//        messagesToDeliver[id].second.erase(firstPair);
//        firstPair = (messagesToDeliver[id].second.begin());
//      }
//
//      // Checks if all messages have been delivered, by checking if the next number exceeds the limits
//      if (messagesToDeliver[id].first > n_proposals) {
//        // If so, deletes the entry from the map
//        messagesToDeliver.erase(id);
//      }
//    }
//  }
//}


bool comparePairs(const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) {
  return a.first < b.first;
}


void Process::doLatticeAgreement() {

  std::vector<std::string> sharedMsgsToDeliver;
  std::mutex sharedMsgsToDeliverMtx;

  std::deque<std::string> newMessagesToBroadcast;
  std::mutex newMessagesToBroadcastMtx;

  LatticeAgreement latticeAgreement(
          std::to_string(processId),
          idToIPAndPort,
          n_proposals,
          myPort,
          sharedMsgsToDeliver,
          sharedMsgsToDeliverMtx,
          newMessagesToBroadcast,
          newMessagesToBroadcastMtx
  );

  // Creates thread that will be submitting the messages to broadcast
  debug("[Process] Creating the thread that will send LatticeAgreement messages from the Process...");
  std::thread tSenderThread(&Process::async_enqueueMessagesToBroadcast, this, std::ref(latticeAgreement));

  // Creates structure that will hold the not-yet-delivered messages (as they need to be delivered in order)
  // Periodically, the sharedMsgsToDeliver will be read and messsaes will be added to this structure.
  // Then, if it's possible to deliver some messages, they will be delivered.
  std::vector<std::pair<int, std::string>> messagesToDeliver;

  // Periodically consumes the delivered messages
  while (1) {
    // Local copy to avoid holding the lock for too long
    // debug("[Process] Waiting for the Mutex for the shared Vector");
    debug("[Process] Creating a local copy of the shared Vector. Waiting for the Mutex for the shared Vector");
    std::vector<std::string> localCopy;

    {
      // Acquire the lock before accessing the shared vector
      std::unique_lock<std::mutex> lock(sharedMsgsToDeliverMtx);
      debug("[Process] Locked the Mutex for the shared Vector");
      localCopy = sharedMsgsToDeliver;
      // Consume the entries
      sharedMsgsToDeliver.clear();
    }
    // debug("[Process] Creating a local copy of the shared Vector. Now iterating over it...");

    // Insert the messages in order
    for (const std::string& message : localCopy) {
      debug("[Process] Processing message: `" + message + "`");

      // Split the message. It's of the form "<runId>:<n1> <n2> ... <nN>"
      std::string runId = message.substr(0, message.find(':'));
      std::string sequenceNumbers = message.substr(message.find(':') + 1);

      // Adds the message to the vector
      messagesToDeliver.push_back(std::make_pair(stoi(runId), sequenceNumbers));
    }
    // Sorts the vector
    std::sort(messagesToDeliver.begin(), messagesToDeliver.end(), comparePairs);

    // Check if this new messages unlocked being able to deliver more
    // debug("[Process] Trying to deliver messages...");
    while (messagesToDeliver.size() > 0 and messagesToDeliver[0].first == lastDeliveredRun + 1) {
      // If so, delivers the messages
      debug("[Process] Delivering the message `" + messagesToDeliver[0].second + "` with runId `" + std::to_string(messagesToDeliver[0].first) + "`");
      std::string sequenceNumbers = messagesToDeliver[0].second;
      (*logsBufferPtr) << sequenceNumbers << std::endl;

      // Every time something is delivered, notifies the other thread, so it can check if it can send more messages
      bufferCv.notify_all();

      // Removes the message from the vector
      messagesToDeliver.erase(messagesToDeliver.begin());
      lastDeliveredRun++;
    }
    debug("[Process] Done delivering messages");
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