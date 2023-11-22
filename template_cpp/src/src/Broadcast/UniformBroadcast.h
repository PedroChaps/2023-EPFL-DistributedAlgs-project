//
// Created by ch4ps on 21/11/23.
//

#ifndef DA_PROJECT_UNIFORMBROADCAST_H
#define DA_PROJECT_UNIFORMBROADCAST_H


#include <set>
#include <string>
#include "../Link/PerfectLink.h"

/**
 * Class that represents a Uniform Broadcast.
 * It will broadcast messages to all processes (including itself) in an uniform and reliable way, implementing the pseudo-code presented in class.
 * When `urbBroadcast(m)` is called, the id of the invoker process, `p_i`, and the id of the original process, `p_j`, are appended to the message before it is sent sent to all processes via a bebBroadcast.
 * So, the crafted message is of the form `<p_i>,<p_j>,<msg>`.
 * Also, when the class is instantiated, a thread is created to listen for messages from other processes.
 */
class UniformBroadcast {

  /**
   * A set that contains the messages that have been urbDelivered.
   * Needed to prevent multiple deliveries of the same message.
   */
  std::set<std::string> delivered;

  /**
   * A set that contains the id+messages that have been relayed (messages of the form `<p_i>,<p_j>,<msg>`).
   * Needed to also prevent multiple relays of the same message.
   */
  std::set<std::string> forwarded;

  /**
   * The processes and the messages they have ACKed.
   * It's a map that looks like this:
   * {
   *    "<message1>": { "<id1>", "<id2>", ...},
   *    "<message2>": { "<id1>", "<id2>", ...},
   *    ...
   * }
   * This structure is necessary to keep track of how many ACKs a message has received.
   * When a message has been ACKed by all processes (ie. when the number of ACKs is the same as the number of processes), it is ready to be urbDelivered.
   */
  std::unordered_map<std::string, std::set<std::string>> acked_msgs;

  /**
   * The id of this process.
   */
  std::string id;

  /**
   * The IP addresses and ports of all the hosts in the system.
   */
  std::vector<std::string> targetIpsAndPorts;

  /**
   * The link used to communicate. Uses the port received as an argument.
   */
  PerfectLink link;

  /**
   * The thread that will receive broadcasts.
   */
  std::thread tReceiver;

  /**
   * The vector that will be shared with the main thread to periodically write.
   */
  std::vector<std::string> &sharedVector;
  std::mutex &sharedVectorMtx;

  /**
   * Best-effort broadcast.
   * Implemented in this class for simplicity.
   * @param msg The message to broadcast.
   */
  void doBebBroadcast(std::string p_j, std::string msg);

public:

  /**
   * Constructor.
   * @param id The id of this process.
   * @param nHosts The number of hosts in the system.
   * @param port The port used to communicate, needed to create a Link.
   */
  UniformBroadcast(std::string id, std::vector<std::string> targetIpsAndPorts, std::string port, std::vector<std::string> &sharedVector, std::mutex &sharedVectorMtx);

  /**
   * Uniform reliable broadcast.
   * @param msg The message to broadcast.
   */
  void doUrbBroadcast(std::string msg);

  /**
   * Receives messages and processes them, eventually calling broadcasts too.
   * Will be called asynchronously, so it must be called in a separate thread.
   */
  void async_receive_broadcasts();

};



#endif //DA_PROJECT_UNIFORMBROADCAST_H
