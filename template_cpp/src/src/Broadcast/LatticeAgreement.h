//
// Created by ch4ps on 17/12/23.
//

#ifndef DA_PROJECT_LATTICEAGREEMENT_H
#define DA_PROJECT_LATTICEAGREEMENT_H

#include <set>
#include <string>
#include "../Link/PerfectLink.h"

/**
 * Class that represents a Lattice Agreement.

 * Messages sent will be of the form:
 * - `<run_id> p <process_id> <round_id> <proposedSet>` for proposals
 * Messages received will be of the form:
 * - `<run_id> a <process_id> <round_id>` for acks, OR
 * - `<run_id> n <process_id> <round_id> <proposedSet>` for nacks.
 * Where:
 * - `<process_id>` is the id of the process that sent the message (useful or optimization).
 * - `<run_id>` is the id of the run.
 * - `<round>` is the round in which the message was sent.
 * - `<proposedSet>` is the set of values proposed by the process. The set is of the form `<nr1>,<nr2>,...,<nrN>`.

 * For simplicity, the initial round is 0

 * Since this is a multi-shot version of the presented pseudo-code, instead of
 *  Integer/Boolean/Set variables, arrays of types are used, where the index of
 *  the array represents the round (eg. `ackCount[3]` represents the number of
 *  acks received for the round 3).
 */
class LatticeAgreement {

  std::vector<bool> active;
  std::vector<int> ackCount;
  std::vector<int> nackCount;
  std::vector<int> activeRound;
  std::vector<std::set<int>> myProposedSet;
  std::mutex myProposedSetMtx;
  std::vector<std::set<int>> acceptedSet;

  /**
   * The number of active runs.
   * Helpful for not having too many active runs at the same time.
   */
  int activeRuns = 0;

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
   * The thread that will receive messages.
   */
  std::thread tReceiver;

  /**
   * The vector that will be shared with the main thread to periodically write
   *  messages to be delivered.
   */
  std::vector<std::string> &sharedMsgsToDeliver;
  std::mutex &sharedMsgsToDeliverMtx;

  /**
   * Messages can be sent in batches of 8.
   * So, to make the algorithm more efficient, messages are not sent as soon as
   * requested. Instead, they are buffered and sent in batches of maximum 8 msgs.
   * For that, two vectors are used as buffers:
   * - `nackMessagesToBroadcast` contains the messages to send because of a NACK.
   *    This have a higher priority and thus will be consumed first (as it
   *    minimizes the number of active runs).
   * - `newMessagesToBroadcast` contains the messages originated from a new run.
   */
  std::vector<std::string> nackMessagesToBroadcast;
  std::mutex nackMsgsToBroadcastMtx;
  std::vector<std::string> &newMessagesToBroadcast;
  std::mutex &newMsgsToBroadcastMtx;

  /**
   * Best-effort broadcast.
   * Implemented in this class (instead of in an unique class) for simplicity.
   * @param msg The message to broadcast.
   */
  void doBebBroadcast(std::string msg);

  /**
   * Transforms a set of integers into a string.
   * The string is of the form `<nr1>,<nr2>,...,<nrN>`.
   * Used when sending a proposal.
   * @param set the set to transform.
   * @return the string representation of the set (form `<nr1>,<nr2>,...,<nrN>`).
   */
  std::string setToString(const std::set<int>& set);

  /**
   * Transforms a string of the form `<nr1>,<nr2>,...,<nrN>` into a set of integers.
   * @param str the string to transform (form `<nr1>,<nr2>,...,<nrN>`).
   * @return the set of integers.
   */
  std::set<int> stringToSet(const std::string& str);


  /*
   * Functions to process received messages.
   * */
  /**
   * Processes a received proposal.
   * @param runId the id of the run.
   * @param processId the id of the process that sent the proposal.
   * @param roundId the id of the round in which the proposal was sent.
   * @param proposedSet the set of values proposed by the process.
   */
  void processProposal(std::string runId, std::string processId, std::string roundId, std::string proposedSet);

  /**
   * Processes a received ACK.
   * @param runId the id of the run.
   * @param processId the id of the process that sent the ACK.
   * @param roundId the id of the round in which the ACK was sent.
   */
  void processAck(std::string runId, std::string processId, std::string roundId);

  /**
   * Processes a received NACK.
   * @param runId the id of the run.
   * @param processId the id of the process that sent the NACK.
   * @param roundId the id of the round in which the NACK was sent.
   * @param proposedSet the set of values proposed by the process.
   */
  void processNack(std::string runId, std::string processId, std::string roundId, std::string proposedSet);

  /**
   * After receiveing a nack/ack, checks for nack/ack conditions (lines 19-26 of pseudocode)
   */
  void doNacksAndAcksChecks();


public:



  /**
   * Constructor.
   * @param id The id of this process.
   * @param targetIpsAndPorts The IP addresses and ports of all the hosts in the system.
   * @param port The port used to communicate, needed to create a Link.
   * @param sharedVector The vector that will be shared with the main thread to periodically write to the file (ie. to deliver messages).
   * @param sharedVectorMtx The mutex used to lock the shared "messages to deliver "vector.
   * @param newMessagesToBroadcast The vector that will be shared with the main thread so it can write the messages that are to be proposed (in different runs).
   * @param newMsgsToBroadcastMtx The mutex used to lock the shared "new messages to broadcast" vector.
   */
  LatticeAgreement(std::string id, std::vector<std::string> targetIpsAndPorts, std::string port, std::vector<std::string> &sharedMsgsToDeliver, std::mutex &sharedMsgsToDeliverMtx, std::vector<std::string> &newMessagesToBroadcast, std::mutex &newMsgsToBroadcastMtx);

  /**
   * Receives messages and processes them, eventually calling broadcasts too.
   * Will be called asynchronously, so it must be called in a separate thread.
   */
  void asyncReceiveMessages();

  // TODO: probably not necessary. Not deleting just in case it is
  /**
   * Sends a proposal.
   * @param proposed_set the set of values to propose.
   */
  void startRound(std::string proposed_set);

};


#endif //DA_PROJECT_LATTICEAGREEMENT_H
