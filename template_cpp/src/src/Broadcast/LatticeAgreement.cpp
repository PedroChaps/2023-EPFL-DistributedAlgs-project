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


// TODO: lock the 2 vectors, create copy vector, unlock the 2 vectors, iterate and
//  send the messages on the copy vector. If there are messages from `newMsgs` vector, things will need to be initialized. Stay permanently in a loop (just wait with thread) waiting for more messages
LatticeAgreement::LatticeAgreement(
        std::string id,
        std::vector<std::string> targetIpsAndPorts,
        std::string port,
        std::vector<std::string> &sharedVector,
        std::mutex &sharedVectorMtx,
        std::vector<std::string> &newMessagesToBroadcast,
        std::mutex &newMsgsToBroadcastMtx) :
        id(id),
        targetIpsAndPorts(targetIpsAndPorts),
        link(PerfectLink(port)),
        sharedMsgsToDeliver(sharedVector),
        sharedMsgsToDeliverMtx(sharedVectorMtx),
        newMessagesToBroadcast(newMessagesToBroadcast),
        newMsgsToBroadcastMtx(newMsgsToBroadcastMtx) {

  // Creates the thread to receive broadcasts
  tReceiver = std::thread(&LatticeAgreement::asyncReceiveMessages, this);
}


// Sends a broadcast to all the processes.
// All the messages bebBroadcasted are of the form `<run_id> p <process_id> <round_id> <myProposedSet>`
//  with `<myProposedSet>` of the form `<nr1>,<nr2>,...,<nrN>`
void LatticeAgreement::doBebBroadcast(std::string msg) {

  for (auto target : targetIpsAndPorts) {
    link.send(msg, target);
  }
}


// TODO: this will kinda be like a switch case, where we have 3 cases:
//  1. The message is a proposal
//  2. The message is an ACK
//  3. The message is a NACK
void LatticeAgreement::asyncReceiveMessages() {

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

    debug("[LatticeAgreement] (receiver) Got the batch of messages: `" + batchOfMessages + "`");
    debug("[LatticeAgreement] (receiver) Extracting stuff... Will be away from reading for a while");

    // A batch of messages, separated by `;` were received. So, iteratively process each one
    std::regex messagePattern(R"((\d+) ([pna]) (\d+) (\d+) ,?([\d,]*)?;?)"); // Regular expression pattern

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
          processProposal(runId, p_i, round_id, proposed_set);
        }
        else if (type == "a" or type == "n") {
          if (not active[stoul(runId)]) {
            debug("[LatticeAgreement] (receiver) Received a proposer message from a run that is not active anymore, ignoring...");
            continue;
          }
          if (type == "a") {
            debug("[LatticeAgreement] Processing an ACK...");
            processAck(runId, p_i, round_id);
          }
          else if (type == "n") {
            debug("[LatticeAgreement] Processing a NACK...");
            processNack(runId, p_i, round_id, proposed_set);
          }
          doNacksAndAcksChecks();
        }
        else {
          debug("[LatticeAgreement] (receiver) Received a broken message with an invalid type: " + type);
        }
      }
    }
  }
}

void LatticeAgreement::processProposal(std::string runId_str, std::string processId, std::string roundId, std::string proposedSet_str) {
  // TODO: implement

  // Converts the string to a set of integers
  std::set<int> proposedSet = stringToSet(proposedSet_str);
  unsigned long runId = stoul(runId_str);

  // If there are less than runId+1 elements in the acceptedSet, it means that Proposer of this round didn't send a proposal yet
  // I need to wait for the round to start because I need to use my proposed set to compare
  if (acceptedSet.size() < runId+1) {
    return;
  }

  {
    std::unique_lock<std::mutex> lock(myProposedSetMtx);
    // TODO: continue
  }

  // TODO: check if I initialized the mutexes
}


void LatticeAgreement::processAck(std::string runId, std::string processId, std::string roundId) {
  // TODO: implement
}


void LatticeAgreement::processNack(std::string runId, std::string processId, std::string roundId, std::string proposedSet) {
  // TODO: implement
}


void LatticeAgreement::doNacksAndAcksChecks() {
  // TODO: implement
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
      if (acked_msgs[message].size() >= targetIpsAndPorts.size()/2 + 1 and delivered.find(message) == delivered.end()) {
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
        debug("[LatticeAgreement] (receiver) Message `" + message +  "` can't be delivered yet, as acked_msgs.size() = " + std::to_string(acked_msgs[message].size()) + ", and targetIpsAndPorts.size() = " + std::to_string(targetIpsAndPorts.size()) + " and delivered.find(message) == delivered.end() is " + std::to_string(static_cast<int>(delivered.find(message) == delivered.end())));
      }


     */

std::string LatticeAgreement::setToString(const std::set<int>& set) {
  std::string result;
  for (int nr : set) {
    result += std::to_string(nr) + ",";
  }
  // Remove the last comma
  result.pop_back();
  return result;
}


std::set<int> LatticeAgreement::stringToSet(const std::string& str) {
  std::set<int> result;
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, ',')) {
    result.insert(std::stoi(token));
  }
  return result;
}

// TODO: probably not necessary. Not deleting just in case it is
void LatticeAgreement::startRound(std::string proposed_set) {

}




