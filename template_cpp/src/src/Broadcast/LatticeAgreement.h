//
// Created by ch4ps on 17/12/23.
//

#ifndef DA_PROJECT_LATTICEAGREEMENT_H
#define DA_PROJECT_LATTICEAGREEMENT_H

#include <set>
#include <string>
#include <deque>
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

  // Pseudo-algorithm variables
  std::vector<bool> active;
  std::vector<std::unique_ptr<std::mutex>> activeMtx;
  std::vector<int> ackCount;
  std::vector<int> nackCount;
  std::vector<int> activeProposalNumber;
  std::vector<std::set<int>> myProposedSet;
  std::mutex myProposedSetMtx; // FIXME: maybe not needed. Remove later
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
   * The IP addresses and ports of all the hosts in the system, mapped by their Id.
   */
  std::unordered_map<std::string,std::string> idToIpAndPort;

  /**
   * The number of processes and the maximum number of faulty ones in the system.
   */
  int nrProcesses;
  int f;

  int n_proposals;

  unsigned long max_nr_vals;

  /**
   * The link used to communicate. Uses the port received as an argument.
   */
  PerfectLink link;

  /**
   * The threads that will receive and send messages.
   */
  std::thread tReceiver;
  std::thread tSender;

  /**
   * The vector that will be shared with the main thread to periodically write
   *  messages to be delivered.
   * Messages will be of the form `<runId> <nr1>,<nr2>,...,<nrN>` (the run id is
   *  necessary to order the deliveries).
   */
  std::vector<std::string> &sharedMsgsToDeliver;
  std::mutex &sharedMsgsToDeliverMtx;

  /**
   * Messages can be sent in batches of 8.
   * So, to make the algorithm more efficient, messages are not sent as soon as
   * requested. Instead, they are buffered and sent in batches of maximum 8 msgs.
   * For that, two vectors are used as buffers:
   * - `ackNackMessagesToSend` contains the messages to send because of an ACK or a NACK. The target id is put in the beginning of the string.
   *    This have a higher priority and thus will be consumed first (as it
   *    minimizes the number of active runs).
   * - `newMessagesToBroadcast` contains the messages originated from a new run.
   */
  std::deque<std::string> ackNackMessagesToSend;
  std::mutex ackNackMessagesToSendMtx;
  std::deque<std::string> &newMessagesToBroadcast;
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
 * Transforms a set of integers into a string in a easier format to be delivered.
 * The string is of the form `<nr1> <nr2> ... <nrN>`.
 * Used when sending a proposal.
 * @param set the set to transform.
 * @return the string representation of the set (form `<nr1> <nr2> ... <nrN>`).
 */
  std::string setToStringDeliveryFormat(const std::set<int>& set);

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
   * @param runId_str the id of the run.
   * @param processId the id of the process that sent the proposal.
   * @param proposalNumber_str the id of the round in which the proposal was sent.
   * @param proposedSet_str the set of values proposed by the process.
   */
  void receiver_processProposal(std::string runId_str, std::string processId, std::string proposalNumber_str, std::string proposedSet_str);

  /**
   * Processes a received ACK.
   * @param runId_str the id of the run.
   * @param processId the id of the process that sent the ACK.
   * @param proposalNumber_str the id of the round in which the ACK was sent.
   */
  void receiver_processAck(std::string runId_str, std::string processId, std::string proposalNumber_str);

  /**
   * Processes a received NACK.
   * @param runId_str the id of the run.
   * @param processId the id of the process that sent the NACK.
   * @param proposalNumber_str the id of the round in which the NACK was sent.
   * @param proposedSet_str the set of values proposed by the process.
   */
  void receiver_processNack(std::string runId_str, std::string processId, std::string proposalNumber_str, std::string proposedSet_str);

  /**
   * After receiveing a nack/ack, checks for nack/ack conditions (lines 19-26 of pseudocode)
   */
  void receiver_doNacksAndAcksChecks(std::string runId_str);

  /**
   * Puts a message in the buffer to be sent (so it can be sent in batches of 8).
   * It has the form `send <targetId>|<msg>`.
   * @param targetId the id of the target process.
   * @param msg the message to send.
   */
  void enqueueToSend(std::string targetId, std::string msg);

  /**
   * Puts a message in the buffer to be broadcasted (so it can be sent in batches of 8).
   * It has the form `broadcast|<msg>`.
   * @param msg the message to broadcast.
   */
  void enqueueToBroadcast(std::string msg);



public:



  /**
   * Constructor.
   * @param id The id of this process.
   * @param idToIpAndPort The IP addresses and ports of all the hosts in the system.
   * @param port The port used to communicate, needed to create a Link.
   * @param sharedVector The vector that will be shared with the main thread to periodically write to the file (ie. to deliver messages).
   * @param sharedVectorMtx The mutex used to lock the shared "messages to deliver "vector.
   * @param newMessagesToBroadcast The vector that will be shared with the main thread so it can write the messages that are to be proposed (in different runs).
   * @param newMsgsToBroadcastMtx The mutex used to lock the shared "new messages to broadcast" vector.
   */
  LatticeAgreement(std::string id, std::unordered_map<std::string,std::string> idToIpAndPort, int n_proposals, unsigned long max_nr_vals, std::string port, std::vector<std::string> &sharedMsgsToDeliver, std::mutex &sharedMsgsToDeliverMtx, std::deque<std::string> &newMessagesToBroadcast, std::mutex &newMsgsToBroadcastMtx);

  /**
   * Receives messages and processes them, eventually calling broadcasts too.
   * Will be called asynchronously, so it must be called in a separate thread.
   */
  void async_ReceiveMessages();

  /**
   * Sends messages that are in the buffer.
   * Will be called asynchronously, so it must be called in a separate thread.
   */
  void async_SendMessages();

  /**
   * Sends a proposal.
   * @param proposed_set the set of values to propose.
   */
  void startRun(int runId, std::string proposedSet_str);

  /**
   * Puts a message in the buffer to be broadcasted (so it can be sent in batches of 8).
   * It has the form `broadcast|<msg>`.
   * @param msg the message to broadcast.
   */
  void enqueueNewMessagesToBroadcast(int runId, std::string proposedSet_str);
};


#endif //DA_PROJECT_LATTICEAGREEMENT_H
