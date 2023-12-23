//
// Created by ch4ps on 17/12/23.
//

#include "LatticeAgreement.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <regex>
#include <algorithm>
#include <unistd.h>

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

#define MSGS_TO_BUFFER 280

LatticeAgreement::LatticeAgreement(
        std::string id,
        std::unordered_map<std::string,std::string> idToIpAndPort,
        int n_proposals,
        unsigned long max_nr_vals,
        std::string port,
        std::vector<std::string> &sharedMsgsToDeliver,
        std::mutex &sharedMsgsToDeliverMtx,
        std::deque<std::string> &newMessagesToBroadcast,
        std::mutex &newMsgsToBroadcastMtx) :
        id(id),
        idToIpAndPort(idToIpAndPort),
        nrProcesses(static_cast<int>(idToIpAndPort.size())),
        f((nrProcesses - 1) / 2),
        n_proposals(n_proposals),
        max_unique_vals(max_nr_vals),
        link(PerfectLink(port)),
        sharedMsgsToDeliver(sharedMsgsToDeliver),
        sharedMsgsToDeliverMtx(sharedMsgsToDeliverMtx),
        newMessagesToBroadcast(newMessagesToBroadcast),
        newMsgsToBroadcastMtx(newMsgsToBroadcastMtx) {

  // Intializes the vectors and sizes them according to the number of proposal messages
  active.resize(static_cast<unsigned long>(n_proposals));
  ackCount.resize(static_cast<unsigned long>(n_proposals));
  nackCount.resize(static_cast<unsigned long>(n_proposals));
  activeProposalNumber.resize(static_cast<unsigned long>(n_proposals));
  myProposedSet.resize(static_cast<unsigned long>(n_proposals));
  acceptedSet.resize(static_cast<unsigned long>(n_proposals));
  alreadyDelivered.resize(static_cast<unsigned long>(n_proposals));

  for (unsigned long i = 0; i < static_cast<unsigned long>(n_proposals); i++) {
    active[i] = false;
    ackCount[i] = 0;
    nackCount[i] = 0;
    activeProposalNumber[i] = 0;
    myProposedSet[i] = std::set<int>();
    acceptedSet[i] = std::set<int>();
    activeMtx.emplace_back(std::make_unique<std::mutex>());
    alreadyDelivered[i] = false;
  }

  // Creates the thread to receive broadcasts
  tReceiver = std::thread(&LatticeAgreement::async_ReceiveMessages, this);
  tSender = std::thread(&LatticeAgreement::async_SendMessages, this);

  // Join the threads
  // tReceiver.join();
  // tSender.join();
}


// Sends a broadcast to all the processes.
// All the messages bebBroadcasted are of the form `<run_id> p <process_id> <round_id> <myProposedSet>`
//  with `<myProposedSet>` of the form `<nr1>,<nr2>,...,<nrN>`
void LatticeAgreement::doBebBroadcast(std::string msg) {

  debug("[LatticeAgreement] (sender) Broadcasting message: `" + msg + "`");
  for (auto target : idToIpAndPort) {
    link.send(msg, target.second);
  }
}


// ----------------------------------------------------------------------------------------------
// Code for the reading of messages and its auxilirary functions
// ----------------------------------------------------------------------------------------------


void LatticeAgreement::async_ReceiveMessages() {

  while (1) {

    // Receive a message through the link.
    // Can be empty if the message is trash (eg. was an ACK, was already received, ...)
    debug("[LatticeAgreement] (receiver) Waiting for a message... Wasn't reading the Link until now");
    std::string batchOfMessages = link.receive();
    debug("[LatticeAgreement] (receiver) Got one!");
    if (batchOfMessages.empty()) {
      debug("[LatticeAgreement] (receiver) It was trash :(");
      continue;
    }

    debug("[LatticeAgreement] (receiver) Wtf man");
    if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Got the batch of messages: `" + batchOfMessages + "`" << std::endl;
    if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Extracting stuff... Will be away from reading for a while" << std::endl;

    // A batch of messages, separated by `;` were received. So, iteratively process each one
    std::regex messagePattern(R"((\d+) ([pna]) (\d+) (\d+) ?,?([\d,]*)?;?)"); // Regular expression pattern

    std::smatch match;
    std::stringstream ss(batchOfMessages);
    std::string msg;

    if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Created the stringstream" << std::endl;

    // Each message is in the format: `<run_id> {p,a,n} <process_id> <round_id> (if {p,n}, <myProposedSet>)`
    // So, extract the run_id and the type of message
    while (std::getline(ss, msg, ';')) {
      if (std::regex_match(msg, match, messagePattern)) {
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Looking at one match" << std::endl;
        std::string runId = match[1];
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) RunId" << runId << std::endl;
        std::string type = match[2];
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) type" << type << std::endl;
        std::string p_i = match[3];
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) p_i" << p_i << std::endl;
        std::string round_id = match[4];
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) round_id" << round_id << std::endl;
        std::string proposed_set = match[5];
        if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Extracted the following - run_id: " + runId + ", type: " + type +
              ", p_i: " + p_i + ", round_id: " + round_id + ", myProposedSet: " + proposed_set << std::endl;

        if (type == "p") {
          // Acceptor role
          if (DEBUG) std::cout << "[LatticeAgreement] Processing a proposal..." << std::endl;
          receiver_processProposal(runId, p_i, round_id, proposed_set);
        }
        else if (type == "a" or type == "n") {
          if (type == "a") {
            if (DEBUG) std::cout << "[LatticeAgreement] Processing an ACK..." << std::endl;
            receiver_processAck(runId, p_i, round_id);
          }
          else if (type == "n") {
            if (DEBUG) std::cout << "[LatticeAgreement] Processing a NACK..." << std::endl;
            receiver_processNack(runId, p_i, round_id, proposed_set);
          }
          if (DEBUG) std::cout << "[LatticeAgreement] Processed the ACK/NACK. Now I will do the ACK/NACK checks" << std::endl;
          receiver_doNacksAndAcksChecks(runId);
          if (DEBUG) std::cout << "[LatticeAgreement] The ACK/NACK checks are now complete!" << std::endl;
        }
        else {
          debug("[LatticeAgreement] (receiver) Received a broken message with an invalid type: " + type);
        }
      }
      if (DEBUG) std::cout << "[LatticeAgreement] (receiver) Finished iterating over the messages of the batch" << std::endl;
    }
  }
}

void LatticeAgreement::receiver_processProposal(std::string runId_str, std::string processId, std::string proposalNumber_str, std::string proposedSet_str) {

  debug("[LatticeAgreement] Received set proposal: " + proposedSet_str);

  // Converts the string to a set of integers
  std::set<int> proposedSet = stringToSet(proposedSet_str);
  unsigned long runId = stoul(runId_str);

  debug("[LatticeAgreement] (receiver) My accepted set was: ");
  std::cout << setToString(acceptedSet[runId]) << std::endl;
  debug("[LatticeAgreement] (receiver) The received set was: ");
  std::cout << setToString(proposedSet) << std::endl;

  // If acceptedSet is a subset or equal to proposedSet, then accept the proposal
  if (std::includes(proposedSet.begin(), proposedSet.end(), acceptedSet[runId].begin(), acceptedSet[runId].end())) {
    debug("[LatticeAgreement] (receiver) I accept the proposal! Received a set that is a superset of the accepted set, so I will accept it");
    acceptedSet[runId] = proposedSet;
    enqueueToSend(processId, runId_str + " a " + id + " " + proposalNumber_str);
  }
  else {
    debug("[LatticeAgreement] (receiver) I don't accept the proposal! Received a set that is not a superset of the accepted set, so I will NACK it");
    std::set<int> unionSet;
    std::set_union(acceptedSet[runId].begin(), acceptedSet[runId].end(), proposedSet.begin(), proposedSet.end(), std::inserter(unionSet, unionSet.begin()));
    acceptedSet[runId] = unionSet;
    debug("[LatticeAgreement] (receiver) Putting in the queue a NACK with proposed set (union of mine with the received one): ");
    if (DEBUG) std::cout << setToString(unionSet) << std::endl;
    enqueueToSend(processId, runId_str + " n " + id + " " + proposalNumber_str + " " + setToString(unionSet));
  }

}


void LatticeAgreement::receiver_processAck(std::string runId_str, std::string processId, std::string proposalNumber_str) {

  debug("[LatticeAgreement] Converting mostRecentRunId and proposal number");
  auto runId = stoul(runId_str);
  auto proposalNumber = stoi(proposalNumber_str);

  // I think this optimizes the code. If I'm not active, it's because I either haven't started or have finished already, so no need to do stuff in this situation.
//  {
//    std::unique_lock<std::mutex> lock(*activeMtx[mostRecentRunId]);
//    if (not active[mostRecentRunId]) {
//      debug("[LatticeAgreement] (receiver) Received a ACK from a run that is not active anymore, ignoring...");
//      return;
//    }
//  }

  debug("[LatticeAgreement] Checking if proposal number is the active one");
  if (DEBUG) std::cout << "proposalNumber: " << proposalNumber << std::endl;
  if (DEBUG) std::cout << "activeProposalNumber[runId]: " << activeProposalNumber[runId] << std::endl;
  if (proposalNumber != activeProposalNumber[runId]) {
    debug("[LatticeAgreement] (receiver) Received an ACK from a round that has passed, ignoring...");
    return;
  }

  debug("[LatticeAgreement] It was active, so I am incrementing the ack count");
  ackCount[runId]++;
}


void LatticeAgreement::receiver_processNack(std::string runId_str, std::string processId, std::string proposalNumber_str, std::string proposedSet_str) {

  auto runId = stoul(runId_str);
  auto proposalNumber = stoi(proposalNumber_str);
  auto proposedSet = stringToSet(proposedSet_str);

//  {
//    std::unique_lock<std::mutex> lock(*activeMtx[mostRecentRunId]);
//    if (not active[mostRecentRunId]) {
//      debug("[LatticeAgreement] (receiver) Received a NACK from a run that is not active anymore, ignoring...");
//      return;
//    }
//  }

  if (proposalNumber != activeProposalNumber[runId]) {
    debug("[LatticeAgreement] (receiver) Received a NACK from a round that has passed, ignoring...");
    return;
  }

  debug("My proposed set was: ");
  if (DEBUG) std::cout << setToString(myProposedSet[runId]) << std::endl;
  debug("The received set was: ");
  if (DEBUG) std::cout << setToString(proposedSet) << std::endl;

  std::set<int> unionSet;
  std::set_union(myProposedSet[runId].begin(), myProposedSet[runId].end(), proposedSet.begin(), proposedSet.end(), std::inserter(unionSet, unionSet.begin()));

  debug("[LatticeAgreement] (receiver) The resulting union (which becomes my new proposed set) is: ");
  if (DEBUG) std::cout << setToString(unionSet) << std::endl;

  myProposedSet[runId] = unionSet;
  nackCount[runId]++;

  if (myProposedSet[runId].size() == max_unique_vals and not alreadyDelivered[runId]) {
    alreadyDelivered[runId] = true;
    debug("[LatticeAgreement] (sender) Shortcut! The proposed set is already the maximum size, so I will enqueue it to be delivered");
    active[runId] = false;
    {
      std::unique_lock<std::mutex> lock(sharedMsgsToDeliverMtx);
      debug("[LatticeAgreement] Locked the mutex. Now I will append the accepted set to the shared vector. Appended message:");
      auto set_str = setToStringDeliveryFormat(myProposedSet[runId]);

      debug("[LatticeAgreement] Enqueuing the following:");
      if (DEBUG) std::cout << runId_str + ":" + set_str << std::endl;

      sharedMsgsToDeliver.push_back(runId_str + ":" + set_str);
    }
    return;
  }
}


void LatticeAgreement::receiver_doNacksAndAcksChecks(std::string runId_str) {

  debug("[LatticeAgreement] Converting mostRecentRunId to unsigned long");
  auto runId = stoul(runId_str);

  {
    debug("[LatticeAgreement] Locking the mutex for the active variable");
    std::unique_lock<std::mutex> lock(*activeMtx[runId]);
    if (not active[runId]) {
      debug("[LatticeAgreement] (receiver) Received a ACK/NACK from a run that is not active anymore, ignoring...");
      return;
    }
  }

  debug("[LatticeAgreement] Doing the `upon` checks of lines 19-23 of pseudocode");
  if (nackCount[runId] > 0 and (static_cast<size_t>(ackCount[runId]) + static_cast<size_t>(nackCount[runId])) >= static_cast<size_t>(f + 1)) {

    debug("[LatticeAgreement] The conditions are met, so I increment the proposal number and broadcast a new proposal");

    activeProposalNumber[runId]++;
    ackCount[runId] = 0;
    nackCount[runId] = 0;

    enqueueToBroadcast(runId_str + " p " + id + " " + std::to_string(activeProposalNumber[runId]) + " " +
                       setToString(myProposedSet[runId]));
  }

  debug("[LatticeAgreement] Doing the `upon` checks of lines 24-26 of pseudocode");
  if (DEBUG) std::cout << "ackCount[runId]: " << ackCount[runId] << std::endl;
  if (DEBUG) std::cout << "f+1: " << f+1 << std::endl;
  if (DEBUG) std::cout << "alreadyDelivered[runId]: " << alreadyDelivered[runId] << std::endl;

  if (static_cast<size_t>(ackCount[runId]) >= static_cast<size_t>(f+1) and not alreadyDelivered[runId]) {

    alreadyDelivered[runId] = true;
    debug("[LatticeAgreement] The condition is met, so I stop the run and deliver my proposed set");
    if (DEBUG) std::cout << setToString(myProposedSet[runId]) << std::endl;

    active[runId] = false;
    {
      std::unique_lock<std::mutex> lock(sharedMsgsToDeliverMtx);
      debug("[LatticeAgreement] Locked the mutex. Now I will append the accepted set to the shared vector. Appended message:");
      auto set_str = setToStringDeliveryFormat(myProposedSet[runId]);

      if (DEBUG) std::cout << runId_str + ":" + set_str << std::endl;

      sharedMsgsToDeliver.push_back(runId_str + ":" + set_str);
    }
  }
}

// ----------------------------------------------------------------------------------------------
// Code for the sending of messages
// ----------------------------------------------------------------------------------------------


// Reads from the vectors some messages (buffering them so the vectors and not constantly being locked/unlocked) and sends them
// according to the type of message
// FIXME: we don't use the batching of 8 messages because I didn't think of this well (they need to be sent to the same process)
// FIXME: temporary solution just to have a baseline running
void LatticeAgreement::async_SendMessages() {

  // creates a copy vector so the original vectors can be unlocked
  std::vector<std::string> updatedProposalsToBroadcast_copy;
  std::vector<std::string> newMsgsToBroadcast_copy;
  std::vector<std::string> ackNackMessagesToSend_copy;

  std::vector<std::string> batchesToBroadcast;
  std::unordered_map<std::string, std::vector<std::string>> batchesToSend;

  while (1) {

    // debug("[LatticeAgreement] (sender) Waiting for the Mutex for the shared Vectors");
    // Extracts messages to the copy vector
    {
      std::unique_lock<std::mutex> lock1(ackNackMessagesToSendMtx);
      std::unique_lock<std::mutex> lock2(newMsgsToBroadcastMtx);
      std::unique_lock<std::mutex> lock3(updatedProposalsToBroadcastMtx);

      // Gets up until `msgsToBuffer` from the vector `ackNackMessagesToSend`, as it has more priority, and then gets the rest from `newMessagesToBroadcast`
      int i = 0;
      while (i < MSGS_TO_BUFFER and not ackNackMessagesToSend.empty()) {
        ackNackMessagesToSend_copy.push_back(ackNackMessagesToSend.front());
        ackNackMessagesToSend.pop_front();
        i++;
      }
      while (i < MSGS_TO_BUFFER and not updatedProposalsToBroadcast.empty()) {
        updatedProposalsToBroadcast_copy.push_back(updatedProposalsToBroadcast.front());
        updatedProposalsToBroadcast.pop_front();
        i++;
      }
      while (i < MSGS_TO_BUFFER and not newMessagesToBroadcast.empty()) {
        newMsgsToBroadcast_copy.push_back(newMessagesToBroadcast.front());
        newMessagesToBroadcast.pop_front();
        i++;
      }
      //      if (ackNackMessagesToSend.size() > 0) {
      //        if (DEBUG) std::cout << "The vector of ACK/NACK msgs to send looks like: " << std::endl;
      //        for (auto compositeMsg: ackNackMessagesToSend) {
      //          if (DEBUG) std::cout << "`" + compositeMsg + "`, " << std::endl;
      //        }
      //        if (DEBUG) std::cout << std::endl;
      //      }
      //      if (newMessagesToBroadcast.size() > 0) {
      //        if (DEBUG) std::cout << "The vector of newMessagesToBroadcast to send looks like: " << std::endl;
      //        for (auto compositeMsg: newMessagesToBroadcast) {
      //          if (DEBUG) std::cout << "`" + compositeMsg + "`, " << std::endl;
      //        }
      //        if (DEBUG) std::cout << std::endl;
      //      }
    }

    // debug("[LatticeAgreement] (sender) Locked the Mutex, unlocked it and have a copy of some messages");
    //    if (messagesToShareCopy.size() > 0) {
    //      if (DEBUG) std::cout << "The vector of messages to be processed looks like: " << std::endl;
    //      for (auto compositeMsg: messagesToShareCopy) {
    //        if (DEBUG) std::cout << "`" + compositeMsg + "`, " << std::endl;
    //      }
    //      if (DEBUG) std::cout << std::endl;
    //    }

    // Does some cool processing to transform individual messages to messages in batches of 8

    // Groups the updated proposals broadcasts in batches of 8, separating each individual message with a `;`
    std::string batchBuilder = "";
    int i = 0;
    for (auto msg : updatedProposalsToBroadcast_copy) {
      // Adding 1000 just to be sure
      if (batchBuilder.length() + msg.length() + 1000 >= BUFFER_SIZE) {
        break;
      }
      if (i == 8) {
        // Remove the last comma
        batchBuilder.pop_back();
        batchesToBroadcast.push_back(batchBuilder);
        batchBuilder = "";
        i = 0;
      }
      batchBuilder += msg + ";";
      i++;
    }
    if (batchBuilder != "" and batchBuilder.length() < BUFFER_SIZE) {
      // Remove the last comma
      batchBuilder.pop_back();
      batchesToBroadcast.push_back(batchBuilder);
    }


    // Groups the sends in batches of 8, separating each individual message with a `;` and grouping them by target
    std::unordered_map<std::string, std::string> batchBuilderMap;
    std::unordered_map<std::string, int> currentBatchSize;
    for (auto msg : ackNackMessagesToSend_copy) {
      std::stringstream ss(msg);
      std::string typeAndTarget;
      std::getline(ss, typeAndTarget, '|');

      if (typeAndTarget.substr(0, 4) != "send") {
        std::cout << "SOMETHING IS VERY WRONG" << std::endl;
        exit(1);
      }

      std::string targetId = typeAndTarget.substr(5); // Skips the `send ` part
      std::string message;
      std::getline(ss, message);

      if (batchBuilderMap[targetId].length() + message.length() + 1000 >= BUFFER_SIZE) {
        break;
      }

      if (currentBatchSize[targetId] == 8) {
        // Remove the last comma
        batchBuilderMap[targetId].pop_back();
        batchesToSend[targetId].push_back(batchBuilderMap[targetId]);
        batchBuilderMap[targetId] = "";
        currentBatchSize[targetId] = 0;
      }

      batchBuilderMap[targetId] += message + ";";
      currentBatchSize[targetId]++;
    }
    for (auto targetAndBatch : batchBuilderMap) {
      if (targetAndBatch.second != "") {
        // Remove the last comma
        targetAndBatch.second.pop_back();
        batchesToSend[targetAndBatch.first].push_back(targetAndBatch.second);
      }
    }


    // Sends the messages, first the single-target and then the proposals
    for (auto targetAndBatches : batchesToSend) {
      for (auto batch : targetAndBatches.second) {
        if (DEBUG) std::cout << "[LatticeAgreement] (sender) Sending a batch of messages to target " + targetAndBatches.first + ": " + batch << std::endl;
        link.send(batch, idToIpAndPort[targetAndBatches.first]);
      }
    }

    for (auto batch : batchesToBroadcast) {
      if (DEBUG) std::cout << "[LatticeAgreement] (sender) Broadcasting a batch of messages (updated proposal): " + batch << std::endl;
      doBebBroadcast(batch);
    }

    // These already come in batches of 8, so just broadcast them
    for (auto batch : newMsgsToBroadcast_copy) {
      if (DEBUG) std::cout << "[LatticeAgreement] (sender) Broadcasting a batch of messages (new): " + batch << std::endl;
      doBebBroadcast(batch);
    }

    updatedProposalsToBroadcast_copy.clear();
    ackNackMessagesToSend_copy.clear();
    newMsgsToBroadcast_copy.clear();

    batchesToBroadcast.clear();
    batchesToSend.clear();
  }

}


// ----------------------------------------------------------------------------------------------
// Auxiliary functions
// ----------------------------------------------------------------------------------------------


std::string LatticeAgreement::setToString(const std::set<int>& set) {

  if (set.empty()) {
    return "";
  }

  std::string result = "";
  for (int nr : set) {
    result += std::to_string(nr) + ",";
  }
  // Remove the last comma
  result.pop_back();
  return result;
}


std::string LatticeAgreement::setToStringDeliveryFormat(const std::set<int>& set) {
  if (set.empty()) {
    return "";
  }

  std::string result = "";
  for (int nr : set) {
    result += std::to_string(nr) + " ";
  }

  // Remove the last space
  result.pop_back();
  return result;
}

// Accepts both the format `<nr1>,<nr2>,...,<nrN>` and `<nr1> <nr2> ... <nrN>`
std::set<int> LatticeAgreement::stringToSet(const std::string& str) {
  std::set<int> result;
  std::stringstream ss(str);
  std::string token;

  char delimiter = ' ';
  if (str.find(',') != std::string::npos) {
    delimiter = ',';
  }

  while (std::getline(ss, token, delimiter)) {
    result.insert(std::stoi(token));
  }

  return result;
}


void LatticeAgreement::startRun(int n_msgs, std::string batchOfSets) {

  debug("[LatticeAgreement] (sender) Starting run with id `" + std::to_string(mostRecentRunId) + "` and batch of sets `" + batchOfSets + "`");

  // For each set_str in the batchOfSets, does the following
  // The separator is `;`
  std::stringstream ss(batchOfSets);
  std::string set_str;
  auto initialRunId = mostRecentRunId;
  while (std::getline(ss, set_str, ';')) {
    debug("[LatticeAgreement] (sender) Initializing stuff regarding set: `" + set_str + "`");

    std::set<int> proposedSet = stringToSet(set_str);
    auto runId_ul = static_cast<unsigned long>(initialRunId);

    {
      std::unique_lock<std::mutex> lock(*activeMtx[runId_ul]);
      myProposedSet[runId_ul] = proposedSet;
      active[runId_ul] = true;
      activeProposalNumber[runId_ul]++;
      ackCount[runId_ul] = 0;
      nackCount[runId_ul] = 0;
    }
    initialRunId++;
  }
  enqueueNewMessagesToBroadcast(mostRecentRunId, mostRecentRunId+n_msgs, batchOfSets);
  mostRecentRunId += n_msgs;
}


void LatticeAgreement::enqueueToSend(std::string targetId, std::string msg) {

  // Locks the mutex for the vector
  {
    std::unique_lock<std::mutex> lock(ackNackMessagesToSendMtx);
    ackNackMessagesToSend.push_back("send " + targetId + "|" + msg);
  }
}


void LatticeAgreement::enqueueToBroadcast(std::string msg) {

  // Locks the mutex for the vector
  {
    std::unique_lock<std::mutex> lock(updatedProposalsToBroadcastMtx);
    updatedProposalsToBroadcast.push_back(msg);
  }
}


void LatticeAgreement::enqueueNewMessagesToBroadcast(int runId, int finalRunId, std::string batchOfSets) {

  // Converts the batch of sets into a vector of messages to broadcast (so it can be accessed with it's index)
  // The separator is `;`
  std::stringstream ss(batchOfSets);
  std::string set_str;
  std::vector<std::string> individualSets;
  while (std::getline(ss, set_str, ';')) {
    individualSets.push_back(set_str);
  }

  // Locks the mutex for the vector
  {
    unsigned long idx = 0;
    std::unique_lock<std::mutex> lock(newMsgsToBroadcastMtx);
    std::string msgsConcatenatedToBroadcast = "";

    for (int i = runId; i < finalRunId; i++) {
      msgsConcatenatedToBroadcast += std::to_string(i) + " p " + id + " 1 " + individualSets[idx] + ";";
      idx++;
    }


    newMessagesToBroadcast.push_back(msgsConcatenatedToBroadcast);
  }
}


