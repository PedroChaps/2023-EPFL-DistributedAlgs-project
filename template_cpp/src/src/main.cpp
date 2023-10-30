#include <chrono>
#include <iostream>
#include <thread>

#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include "Process/Receiver.h"
#include "Process/Sender.h"
#include "Link/PerfectLink.h"


#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

struct ConfigValues {
  unsigned long m; // number of messages to send
  unsigned long i; // index of the receiver Process
};

struct IpAndPort {
  std::string ip;
  std::string port;
};

// Global variables are necessary for writting in the logs in case of a signal
std::stringstream logsBuffer;
std::string logsPath;

ConfigValues readConfigFile(std::string& configPath);
IpAndPort parseHostsFileById(std::vector<Parser::Host> hosts, unsigned long id);

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  std::cout << "I'm ded.\n";

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";
  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << logsBuffer.str();
  logFile.close();

  // Clears the buffer
  logsBuffer.str("");

  // exit directly from signal handler
  exit(0);
}

static void displayInitialInfo(Parser parser){

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT "
  << getpid() << "` or `kill -SIGTERM "
  << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";
}


ConfigValues readConfigFile(std::string& configPath) {

  std::ifstream file(configPath);
  ConfigValues result{};

  if (file.is_open()) {
    std::string line;
    if (std::getline(file, line)) {
      std::istringstream iss(line);
      if (iss >> result.m >> result.i) {
        return result;
      } else {
        std::cerr << "Error: Could not read two numbers from the file." << std::endl;
      }
    } else {
      std::cerr << "Error: File is empty." << std::endl;
    }
    file.close();
  } else {
    std::cerr << "Error: Unable to open the file." << std::endl;
  }

  // If there was an error or the file couldn't be read, return default values.
  return {0, 0};

}


IpAndPort parseHostsFileById(std::vector<Parser::Host> hosts, unsigned long id) {

  for (auto &host : hosts) {
    if (host.id == id) {

      struct in_addr addr;
      addr.s_addr = host.ip;

      return {inet_ntoa(addr), std::to_string(static_cast<unsigned int>(host.port))};
    }
  }

  // If there was an error or the file couldn't be read, return default values.
  return {"", ""};

}


int main(int argc, char **argv) {

  // TODO: idk what this does, confirm
  signal(SIGTERM, stop);
  signal(SIGINT, stop);
  logsBuffer.str("");

  // TODO: check what this is
  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  displayInitialInfo(parser);

  std::cout << "Doing some initialization...\n\n";
  auto id = parser.id();
  auto hosts = parser.hosts();
  logsPath = parser.outputPath();
  std::string configPath = parser.configPath();

  std::cout << "Reading configPath and determining type...\n\n";

  auto configValues = readConfigFile(configPath);

  // Given the config file's id, we know the id of the receiver.
  // We can use this to get the receiver's ip and port by using the hosts file.
  auto receiverIpAndPort = parseHostsFileById(hosts, configValues.i);

  std::string receiverIp = receiverIpAndPort.ip;
  std::string receiverPort = receiverIpAndPort.port;
  debug("[main] Receiver IP: " + receiverIp);
  debug("[main] Receiver Port: " + receiverPort);

  if (configValues.i == id) {
    std::cout << "I am a receiver!\n\n";

    Receiver receiver(receiverPort, logsPath, &logsBuffer);
    receiver.receiveBroadcasts();
  }
  else {
    std::cout << "I am a sender!\n\n";

    Sender sender(receiverIp, receiverPort, logsPath, &logsBuffer, static_cast<int>(configValues.m), static_cast<int>(id));
    sender.sendBroadcasts();
  }

  std::cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}