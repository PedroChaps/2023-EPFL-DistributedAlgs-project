//
// Created by ch4ps on 21/11/23.
//

#include <iostream>
#include "UniformBroadcast.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

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

UniformBroadcast::UniformBroadcast(std::string id, std::vector<std::string> targetIpsAndPorts, std::string port, std::vector<std::string> &sharedVector, std::mutex &sharedVectorMtx) :
        id(id), targetIpsAndPorts(targetIpsAndPorts), link(PerfectLink(port)), sharedVector(sharedVector), sharedVectorMtx(sharedVectorMtx) {

  // Creates the thread to receive broadcasts
  tReceiver = std::thread(&UniformBroadcast::async_receive_broadcasts, this);
}


// Sends a broadcast to all the processes.
// All the messages bebBroadcasted are of the form `<p_i>,<p_j>,<msg>`
void UniformBroadcast::doBebBroadcast(std::string p_j, std::string msg) {

  std::string idsAndMessage = id + "," + p_j + "," + msg;

  for (auto target : targetIpsAndPorts) {
    link.send(idsAndMessage, target);
  }
}


void UniformBroadcast::doUrbBroadcast(std::string msg) {

  debug("[UniformBroadcast] urbBroadcasting message: `" + msg + "`");

  // Saves it in the forwarded messages
  forwarded.insert(id + "," + msg);

  // Sends it to all the processes
  debug("[UniformBroadcast] bebBroadcasting message: `" + msg + "`");
  doBebBroadcast(id, msg);
}


void UniformBroadcast::async_receive_broadcasts() {

  while (1) {

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    debug("[UniformBroadcast] (receiver) Waiting for a message... Wasn't reading the Link until now");
    // Receive a message through the link.
    // Can be empty if the message is trash (eg. was an ACK, was already received, ...)
    std::string idAndMessage = link.receive();
    debug("[UniformBroadcast] (receiver) Got one!");
    if (idAndMessage.empty()) {
      debug("[UniformBroadcast] (receiver) It was trash :(");
      continue;
    }

    debug("[UniformBroadcast] (receiver) Extracting stuff... Will be away from reading for a while");
    // The message is in the format: `<p_i>,<p_j>,<msg>`
    std::string p_i = idAndMessage.substr(0, idAndMessage.find(','));
    std::string p_j = idAndMessage.substr(idAndMessage.find(',') + 1, idAndMessage.find(',', idAndMessage.find(',') + 1) - idAndMessage.find(',') - 1);
    std::string message = idAndMessage.substr(idAndMessage.find(',', idAndMessage.find(',') + 1) + 1);

    debug("[UniformBroadcast] (receiver) Received message: `" + message + "` from process `" + p_i + "` that originated from process `" + p_j + "`");

    acked_msgs[message].insert(p_i);

    std::string p_jAndMessage = p_j + "," + message;

    // Checks if the message was not forwarded already. If not, it's a new message, so it needs to be forwarded.
    if (forwarded.find(p_jAndMessage) == forwarded.end()) {
      debug("[UniformBroadcast] (receiver) Message was not forwarded yet, forwarding...");
      forwarded.insert(p_jAndMessage);
      doBebBroadcast(p_j, message);
    } else {
      debug("[UniformBroadcast] (receiver) Message was already forwarded!");
    }

    // Checks if the message can be delivered by checking if a majority has ACKed
    if (acked_msgs[message].size() >= targetIpsAndPorts.size()/2 + 1 and delivered.find(message) == delivered.end()) {
      debug("[UniformBroadcast] (receiver) Message can be delivered, delivering...");

      // Adds the message to the delivered set
      delivered.insert(message);

      // Delivers the message
      {
        debug("[Process] Waiting for the Mutex for the shared Vector");
        std::unique_lock<std::mutex> lock(sharedVectorMtx);
        debug("[Process] Locked the Mutex for the shared Vector");
        sharedVector.push_back(message);
        debug("[UniformBroadcast] (receiver) Message delivered (ie. appended to the vector): `" + message + "`");
      }
      debug("[Process] Unlocked the Mutex for the shared Vector");
    } else {
      debug("[UniformBroadcast] (receiver) Message `" + message +  "` can't be delivered yet, as acked_msgs.size() = " + std::to_string(acked_msgs[message].size()) + ", and targetIpsAndPorts.size() = " + std::to_string(targetIpsAndPorts.size()) + " and delivered.find(message) == delivered.end() is " + std::to_string(static_cast<int>(delivered.find(message) == delivered.end())));
    }

  }

}
