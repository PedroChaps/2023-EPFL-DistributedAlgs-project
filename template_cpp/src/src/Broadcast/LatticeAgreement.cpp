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

#define MSGS_TO_BUFFER 35

LatticeAgreement::LatticeAgreement(
        std::string id,
        std::unordered_map<std::string,std::string> idToIpAndPort,
        int n_proposals,
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

  for (unsigned long i = 0; i < static_cast<unsigned long>(n_proposals); i++) {
    active[i] = false;
    ackCount[i] = 0;
    nackCount[i] = 0;
    activeProposalNumber[i] = 0;
    myProposedSet[i] = std::set<int>();
    acceptedSet[i] = std::set<int>();
    activeMtx.emplace_back(std::make_unique<std::mutex>());
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
    std::cout << "[LatticeAgreement] (receiver) Got the batch of messages: `" + batchOfMessages + "`" << std::endl;
    debug("[LatticeAgreement] (receiver) Extracting stuff... Will be away from reading for a while");

    // A batch of messages, separated by `;` were received. So, iteratively process each one
    std::regex messagePattern(R"((\d+) ([pna]) (\d+) (\d+) ?,?([\d,]*)?;?)"); // Regular expression pattern

    std::smatch match;
    std::stringstream ss(batchOfMessages);
    std::string msg;

    // Each message is in the format: `<run_id> {p,a,n} <process_id> <round_id> (if {p,n}, <myProposedSet>)`
    // So, extract the run_id and the type of message
    while (std::getline(ss, msg, ';')) {
      if (std::regex_match(msg, match, messagePattern)) {
        std::string runId = match[1];
        std::string type = match[2];
        std::string p_i = match[3];
        std::string round_id = match[4];
        std::string proposed_set = match[5];
        debug("[LatticeAgreement] (receiver) Extracted the following - run_id: " + runId + ", type: " + type +
              ", p_i: " + p_i + ", round_id: " + round_id + ", myProposedSet: " + proposed_set);

        if (type == "p") {
          // Acceptor role
          debug("[LatticeAgreement] Processing a proposal...");
          receiver_processProposal(runId, p_i, round_id, proposed_set);
        }
        else if (type == "a" or type == "n") {
          if (type == "a") {
            debug("[LatticeAgreement] Processing an ACK...");
            receiver_processAck(runId, p_i, round_id);
          }
          else if (type == "n") {
            debug("[LatticeAgreement] Processing a NACK...");
            receiver_processNack(runId, p_i, round_id, proposed_set);
          }
          debug("[LatticeAgreement] Processed the ACK/NACK. Now I will do the ACK/NACK checks");
          receiver_doNacksAndAcksChecks(runId);
          debug("[LatticeAgreement] Now I will do the ACK/NACK checks are now complete!");
        }
        else {
          debug("[LatticeAgreement] (receiver) Received a broken message with an invalid type: " + type);
        }
      }
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
    debug("[LatticeAgreement] (receiver) Sending NACK with proposed set (union of mine with the received one): ");
    if (DEBUG) std::cout << setToString(unionSet) << std::endl;
    enqueueToSend(processId, runId_str + " n " + id + " " + proposalNumber_str + " " + setToString(unionSet));
  }

}


void LatticeAgreement::receiver_processAck(std::string runId_str, std::string processId, std::string proposalNumber_str) {

  debug("[LatticeAgreement] Converting runId and proposal number");
  auto runId = stoul(runId_str);
  auto proposalNumber = stoi(proposalNumber_str);

  // I think this optimizes the code. If I'm not active, it's because I either haven't started or have finished already, so no need to do stuff in this situation.
//  {
//    std::unique_lock<std::mutex> lock(*activeMtx[runId]);
//    if (not active[runId]) {
//      debug("[LatticeAgreement] (receiver) Received a ACK from a run that is not active anymore, ignoring...");
//      return;
//    }
//  }

  debug("[LatticeAgreement] Checking if proposal number is the active one");
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
//    std::unique_lock<std::mutex> lock(*activeMtx[runId]);
//    if (not active[runId]) {
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
}


void LatticeAgreement::receiver_doNacksAndAcksChecks(std::string runId_str) {

  debug("[LatticeAgreement] Converting runId to unsigned long");
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
  if (static_cast<size_t>(ackCount[runId]) >= static_cast<size_t>(f+1)) {

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
/*
      std::string p_i = idAndMessage.substr(0, idAndMessage.find(','));
      std::string p_j = idAndMessage.substr(idAndMessage.find(',') + 1, idAndMessage.find(',', idAndMessage.find(',') + 1) - idAndMessage.find(',') - 1);
      std::string message = idAndMessage.substr(idAndMessage.find(',', idAndMessage.find(',') + 1) + 1);

      debug("[LatticeAgreement] (receiver) Received message: `" + message + "` from process `" + p_i + "` that originated from process `" + p_j + "`");


      acked_msgs[message].insert(p_i);

      std::string p_jAndMessage = p_j + "," + message;

      // Checks if the message was not forwarded already. If not, it's a new message, so it needs to be forwarded.
      if (forwarded.find(p_jAndMessage) == forwarded.end()) {
        debug("[LatticeAgreement] (receiver) Message was not forwarded yet, forwarding...");
        forwarded.insert(p_jAndMessage);
        doBebBroadcast(message);
      } else {
        debug("[LatticeAgreement] (receiver) Message was already forwarded!");
      }

      // Checks if the message can be delivered by checking if a majority has ACKed
      if (acked_msgs[message].size() >= idToIpAndPort.size()/2 + 1 and delivered.find(message) == delivered.end()) {
        debug("[LatticeAgreement] (receiver) Message can be delivered, delivering...");

        // Adds the message to the delivered set
        delivered.insert(message);

        // Delivers the message
        {
          debug("[Process] Waiting for the Mutex for the shared Vector");
          std::unique_lock<std::mutex> lock(sharedMsgsToDeliverMtx);
          debug("[Process] Locked the Mutex for the shared Vector");
          sharedMsgsToDeliver.push_back(message);
          debug("[LatticeAgreement] (receiver) Message delivered (ie. appended to the vector): `" + message + "`");
        }
        debug("[Process] Unlocked the Mutex for the shared Vector");
      } else {
        debug("[LatticeAgreement] (receiver) Message `" + message +  "` can't be delivered yet, as acked_msgs.size() = " + std::to_string(acked_msgs[message].size()) + ", and idToIpAndPort.size() = " + std::to_string(idToIpAndPort.size()) + " and delivered.find(message) == delivered.end() is " + std::to_string(static_cast<int>(delivered.find(message) == delivered.end())));
      }


     */


// ----------------------------------------------------------------------------------------------
// Code for the sending of messages
// ----------------------------------------------------------------------------------------------


// Reads from the vectors some messages (buffering them so the vectors and not constantly being locked/unlocked) and sends them
// according to the type of message
// FIXME: we don't use the batching of 8 messages because I didn't think of this well (they need to be sent to the same process)
// FIXME: temporary solution just to have a baseline running
void LatticeAgreement::async_SendMessages() {

  // creates a copy vector so the original vectors can be unlocked
  std::vector<std::string> messagesToShareCopy;

  while (1) {

    // debug("[LatticeAgreement] (sender) Waiting for the Mutex for the shared Vectors");
    // Extracts messages to the copy vector
    {
      std::unique_lock<std::mutex> lock1(ackNackMessagesToSendMtx);
      std::unique_lock<std::mutex> lock2(newMsgsToBroadcastMtx);

      // Gets up until `msgsToBuffer` from the vector `ackNackMessagesToSend`, as it has more priority, and then gets the rest from `newMessagesToBroadcast`
      int i = 0;
      while (i < MSGS_TO_BUFFER and not ackNackMessagesToSend.empty()) {
        messagesToShareCopy.push_back(ackNackMessagesToSend.front());
        ackNackMessagesToSend.pop_front();
        i++;
      }
      while (i < MSGS_TO_BUFFER and not newMessagesToBroadcast.empty()) {
        messagesToShareCopy.push_back(newMessagesToBroadcast.front());
        newMessagesToBroadcast.pop_front();
        i++;
      }
    }
    // debug("[LatticeAgreement] (sender) Locked the Mutex, unlocked it and have a copy of some messages");

    // Sends the messages
    for (auto compositeMsg : messagesToShareCopy) {
      // The message is of the form `send <target_id>|<message>` or `broadcast|<message>`
      std::stringstream ss(compositeMsg);
      std::string type;
      std::getline(ss, type, '|');
      if (type == "broadcast") {
        // Of the form `broadcast|<message>`
        std::string message;
        std::getline(ss, message);
        debug("[LatticeAgreement] (sender) Broadcasting `" + message + "`");
        doBebBroadcast(message);
      }
      else if (type.substr(0, 4) == "send") {
        // Of the form `send <target_id>|<message>`
        std::string targetId = type.substr(5);
        std::string message;
        std::getline(ss, message);
        debug("[LatticeAgreement] (sender) Sending `" + message + "` to `" + targetId + "`");
        link.send(message, idToIpAndPort[targetId]);
      }
      else {
        debug("[LatticeAgreement] (sender) Received a broken message with an invalid type: " + type);
      }
    }
    messagesToShareCopy.clear();
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


void LatticeAgreement::startRun(int runId, std::string proposedSet_str) {

  debug("[LatticeAgreement] (sender) Starting run with id `" + std::to_string(runId) + "` and proposedSet `" + proposedSet_str + "`");

  std::set<int> proposedSet = stringToSet(proposedSet_str);
  auto runId_ul = static_cast<unsigned long>(runId);

  {
    std::unique_lock<std::mutex> lock(*activeMtx[runId_ul]);
    myProposedSet[runId_ul] = proposedSet;
    active[runId_ul] = true;
    activeProposalNumber[runId_ul]++;
    ackCount[runId_ul] = 0;
    nackCount[runId_ul] = 0;
  }

  // Convert the input set to the used format (ie. from `<nr1> <nr2> ... <nrN>` to `<nr1>,<nr2>,...,<nrN>`)
  std::string proposedSet_str2 = setToString(proposedSet);

  // Generate the broadcast message and save it in the newMessages
  enqueueNewMessagesToBroadcast(runId, proposedSet_str2);
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
    std::unique_lock<std::mutex> lock(ackNackMessagesToSendMtx);
    ackNackMessagesToSend.push_back("broadcast|" + msg);
  }
}


void LatticeAgreement::enqueueNewMessagesToBroadcast(int runId, std::string proposedSet_str) {

  // Locks the mutex for the vector
  {
    std::unique_lock<std::mutex> lock(newMsgsToBroadcastMtx);
    newMessagesToBroadcast.push_back("broadcast|" + std::to_string(runId) + " p " + id + " 1 " + proposedSet_str);
  }
}


