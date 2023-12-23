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
#include <regex>

#define DELTA_RUNS 8

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
Process::Process(std::string myPort, int p, unsigned long max_unique_vals, int nHosts, int processId, std::unordered_map<std::string,std::string> idToIPAndPort, std::string configPath, std::string logsPath, std::stringstream *logsBuffer) :
        processId(processId), idToIPAndPort(idToIPAndPort), myPort(myPort), n_proposals(p), max_unique_vals(max_unique_vals), configPath(configPath), logsPath(logsPath) {

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
        return i <= n_proposals and lastBroadcastedRun - lastDeliveredRun < DELTA_RUNS;
      });
      debug("[Process] (sender) Passed the CV");
      // Reads the next line
      std::string batch;

      unsigned int n_msgs = 0;
      for (int j = 0; j < 8; j++) {
        if (std::getline(file, line)) {
          batch += line + ";";
          n_msgs++;
        } else if (line.empty()){
          break;
        }
      }

      // Case where the file became empty on the last run
      if (n_msgs == 0) {
        break;
      }

      // Convert the batch from `nr1 nr2 ... nrN;nr1 nr2 ... nrN;...` to `nr1,nr2,...,nrN;nr1,nr2,...,nrN`
      std::regex regex(" ");
      batch = std::regex_replace(batch, regex, ",");
      if (DEBUG) std::cout << "[Process] (sender) Created the batch: `" + batch + "`" << std::endl;
      if (DEBUG) std::cout << "[Process] (sender) Starting runs " + std::to_string(i) + " to " + std::to_string(
                static_cast<unsigned int>(i) + n_msgs) + " with input set `" + batch + "`" << std::endl;

      latticeAgreement.startRun(static_cast<int>(n_msgs), batch);

      // Periodically saves the logs
      if (i % 240 == 0) {
        {
          std::lock_guard<std::mutex> lock_logs(logsBufferMtx);
          saveLogs();
        }
      }
      i+=n_msgs;
      lastBroadcastedRun+=n_msgs;
    }
  }

  debug("[Process] (sender) Done sending messages! I can relax :D");
}


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
          max_unique_vals,
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
  std::deque<std::pair<int, std::string>> messagesToDeliver;

  // Periodically consumes the delivered messages
  while (1) {
    // Local copy to avoid holding the lock for too long
    // debug("[Process] Waiting for the Mutex for the shared Vector");
    std::vector<std::string> localCopy;

    {
      // Acquire the lock before accessing the shared vector
      std::unique_lock<std::mutex> lock(sharedMsgsToDeliverMtx);
      localCopy = sharedMsgsToDeliver;
      // Consume the entries
      sharedMsgsToDeliver.clear();
    }
    // debug("[Process] Creating a local copy of the shared Vector. Now iterating over it...");

    // Insert the messages in order
    for (const std::string& message : localCopy) {
      if (DEBUG) std::cout << "Processing message: `" << message << "`" << std::endl;

      // Split the message. It's of the form "<mostRecentRunId>:<n1> <n2> ... <nN>"
      std::string runId = message.substr(0, message.find(':'));
      std::string sequenceNumbers = message.substr(message.find(':') + 1);

//      if (DEBUG) std::cout << "Run id: `" << mostRecentRunId << "`" << std::endl;
//      if (DEBUG) std::cout << "sequenceNumbers: `" << sequenceNumbers << "`" << std::endl;

      // Adds the message to the vector
      messagesToDeliver.emplace_back(stoi(runId), sequenceNumbers);
    }
    // Sorts the vector
    std::sort(messagesToDeliver.begin(), messagesToDeliver.end(), comparePairs);

    // Check if this new messages unlocked being able to deliver more
    // debug("[Process] Trying to deliver messages...");
//    if (DEBUG) std::cout << "messagesToDeliver[0].first: `" << messagesToDeliver[0].first << "`" << std::endl;
//    if (DEBUG) std::cout << "lastDeliveredRun: `" << lastDeliveredRun << "`" << std::endl;
//    if (DEBUG) {
//      for (auto &msg : messagesToDeliver) {
//        std::cout << "messagesToDeliver: `" << msg.first << "` `" << msg.second << "`" << std::endl;
//      }
//    }
    while (!messagesToDeliver.empty() and messagesToDeliver[0].first == lastDeliveredRun + 1) {
      // If so, delivers the messages
      if (DEBUG) std::cout << "Delivering the message `" << messagesToDeliver[0].second << "` with mostRecentRunId `" << std::to_string(messagesToDeliver[0].first) << "`" << std::endl;

      std::string sequenceNumbers = messagesToDeliver[0].second;
      {
        std::lock_guard<std::mutex> lock_logs(logsBufferMtx);
        (*logsBufferPtr) << sequenceNumbers << std::endl;
      }

      // Every time something is delivered, notifies the other thread, so it can check if it can send more messages
      bufferCv.notify_all();

      // Removes the message from the vector
      messagesToDeliver.pop_front();
      lastDeliveredRun++;
    }
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