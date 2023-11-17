//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_SENDER_H
#define DA_PROJECT_SENDER_H

#include <string>
#include <sstream>
#include "../Link/PerfectLink.h"

/**
 * Class that represents a Sender Process.
 * It will send batches of messages to another process.
 */
class Sender {

  /**
   * The port used to send messages.
   */
  std::string myPort;

  /**
   * The IP address and port of the receiver Process.
   */
  std::vector<std::string> targetIps;
  std::vector<std::string> targetPorts;

  /**
   * The path to the output file.
   */
  std::string logsPath;

  /**
   * The buffer used to store the logs.
   */
  std::stringstream *logsBufferPtr;

  /**
   * The number of messages to send.
   */
  int m;
  int nHosts;

  /**
   * The id of this process, sent in the messages.
   */
  int processId;

  /**
   * The Perfect Link used to send messages.
   */
  std::vector<PerfectLink> links;

public:

  /**
   * Constructor.
   * @param ipAddress The IP address of the receiver Process.
   * @param port The port of the receiver Process.
   * @param logsPath The path to the output file.
   * @param logsBuffer The buffer used to store the logs.
   * @param m The number of messages to send.
   * @param processId The id of this process, sent in the messages.
   */
  Sender(std::vector<std::string> targetIps, std::vector<std::string> targetPorts, std::string myPort, std::string logsPath, std::stringstream *logsBuffer, int m, int nHosts, int processId);

  /**
   * With the use of a PerfectLink, sends batches of messages to the destiny.
   * */
  void sendBroadcasts();

  /**
   * Saves whatever is in `logsBuffer` to the file pointed by `logsPath`
   */
  void saveLogs();
};

#endif //DA_PROJECT_SENDER_H
