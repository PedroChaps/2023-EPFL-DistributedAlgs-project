//
// Created by ch4ps on 04-10-2023.
//

#ifndef DA_PROJECT_RECEIVER_H
#define DA_PROJECT_RECEIVER_H

#include <string>
#include <sstream>
#include "../Link/PerfectLink.h"

/**
 * Class that represents a Receiver Process.
 * It will receive messages from other Processes and deliver them to the main application, by printing them to the console (and writting them to the output file).
 */
class Receiver {

  /**
   * The Perfect Link used to receive messages.
   */
  PerfectLink link;

  /**
   * The port used to receive messages.
   */
  std::string port;

  /**
   * The path to the output file.
   */
  std::string logsPath;

  /**
   * The buffer used to store the logs.
   */
  std::stringstream *logsBufferPtr;

public:

  /**
   * Constructor.
   * @param port The port used to receive messages.
   * @param logsPath The path to the output file.
   * @param logsBuffer The buffer used to store the logs.
   */
  Receiver(std::string port, std::string logsPath, std::stringstream *logsBuffer);

  /**
   * With the use of a PerfectLink, remains actively listening for broadcasts (i.e. messages) from Sender processes.
   */
  void receiveBroadcasts();
};


#endif //DA_PROJECT_RECEIVER_H
