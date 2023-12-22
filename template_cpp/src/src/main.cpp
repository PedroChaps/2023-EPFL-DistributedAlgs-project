#include <chrono>
#include <iostream>
#include <thread>

#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include "Process/Receiver.h"
#include "Process/Sender.h"
#include "Process/Process.h"
#include "Link/PerfectLink.h"
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


// Global variables are necessary for writting in the logs in case of a signal
std::stringstream logsBuffer;
std::string logsPath;

// Struct to facilitate the reading of the config file
struct ConfigValues {
  unsigned long p; // number of proposals
  unsigned long vs; // maximum number of elements in a proposal
  unsigned long ds; // max number of distinct elements across all proposals
  std::vector<std::string> inputSets;
};

// Some declaration of functions
ConfigValues readConfigFile(std::string& configPath);
std::unordered_map<std::string,std::string> parseHostsFile(std::vector<Parser::Host> hosts, unsigned long id, std::string &myPort);

// Function to handle the SIGINT and SIGTERM signals
// It will write the logs to the output file before stopping the program
static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  std::cout << "I'm ded." << std::endl;
  std::cout << "------------------" << std::endl;

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing." << std::endl;

  // write/flush output file if necessary
  std::cout << "Writing output to the log file." << std::endl;
  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << logsBuffer.str();
  logFile.close();

  // Clears the buffer
  logsBuffer.str("");

  std::cout << "Exiting the program." << std::endl;
  // exit directly from signal handler
  exit(0);
}

// Function to display the initial information
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

// Function to read the config file
ConfigValues readConfigFile(std::string& configPath) {
  std::ifstream file(configPath);
  ConfigValues values;

  if (file.is_open()) {
    std::string line;
    if (std::getline(file, line)) {
      std::istringstream iss(line);
      iss >> values.p >> values.vs >> values.ds;
    }

    // TODO: remove (bcz it's read in Process)
    while (std::getline(file, line)) {
      values.inputSets.push_back(line);
    }

    file.close();
  } else {
    std::cerr << "Unable to open file: " << configPath << std::endl;
  }

  return values;
}

// Function to parse the hosts file.
// It will return a map of ids to their ips and the respective ports, in the format `<ip>:<port>` (so, the map will look like
// `{"1": "127.0.0.1:12345", "2": "127.0.0.1:67890", ...}`.
std::unordered_map<std::string,std::string> parseHostsFile(std::vector<Parser::Host> hosts, unsigned long id, std::string &myPort) {

  std::unordered_map<std::string,std::string> idToIpAndPort;

  for (auto &host : hosts) {
    struct in_addr addr;
    addr.s_addr = host.ip;

    std::string ip = inet_ntoa(addr);
    unsigned int port = static_cast<unsigned int>(host.port);

    if (host.id == id) {
      myPort = std::to_string(port);
    }
    idToIpAndPort.emplace(std::to_string(host.id), ip + ":" + std::to_string(port));
  }

  return idToIpAndPort;
}


int main(int argc, char **argv) {

  signal(SIGTERM, stop);
  signal(SIGINT, stop);
  logsBuffer.str("");

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
  std::string myPort;
  auto idToIpAndPort = parseHostsFile(hosts, id, myPort);
  int nHosts = static_cast<int>(hosts.size())-1;

  // Based on the config file's id, we know if this process is a sender or a receiver.
  // Proceeds accordingly.
  std::cout << "I am a process!\n\n";

  // sleep(30);

  // PerfectLink link(myPort);
  // Process process(link, myPort, logsPath, &logsBuffer, static_cast<int>(configValues.m), nHosts, static_cast<int>(id), idToIpAndPort);

  Process process(myPort, static_cast<int>(configValues.p), nHosts, static_cast<int>(id), idToIpAndPort, configPath, logsPath, &logsBuffer);

  process.doLatticeAgreement();

  std::cout << "My job here is done. Waiting for my termination...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}