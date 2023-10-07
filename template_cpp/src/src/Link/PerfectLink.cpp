//
// Created by ch4ps on 04-10-2023.
//

#include "PerfectLink.h"

void PerfectLink::send(std::string message) {

  // TODO: wait for ACK of send for RETRANSMISSION_TIMEOUT milliseconds
  for (int tries = 0; tries < MAX_RETRANSMISSIONS; tries++) {

    Link::send(message);
  }

}

void PerfectLink::send(std::vector<std::string> messages) {
  //TODO: implement
}