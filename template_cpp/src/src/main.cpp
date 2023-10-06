#include <chrono>
#include <iostream>
#include <thread>

#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include "Process/Process.h"
#include "Link/Link.h"


struct ConfigValues {
  unsigned long m; // number of messages to send
  unsigned long i; // index of the receiver Process
};

ConfigValues readConfigFile(std::string& configPath);

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

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


int main(int argc, char **argv) {

  // TODO: idk what this does, confirm
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  displayInitialInfo(parser);

  std::cout << "Doing some initialization...\n\n";
  auto id = parser.id();
  auto hosts = parser.hosts();
  std::string outputPath = parser.outputPath();
  std::string configPath = parser.configPath();

  std::cout << "Reading configPath and determining type...\n\n";

  auto configValues = readConfigFile(configPath);
  if (configValues.i == id) {
    std::cout << "I am a receiver!\n\n";
    std::string ip = "localhost";
    std::string port1 = "12345";
    std::string port2 = "12346";

    Link link1_receiver(0, 1, RECEIVER, port1);
    std::cout << "Waiting message...\n";
    while (1) {
      std::string received = link1_receiver.receive();
      std::cout << "Received: " << received << "\n";
    }
    std::cout << "Exiting...\n";
    exit(1);
  }
  else {
    std::cout << "I am a sender!\n\n";
    std::string ip = "localhost";
    std::string port1 = "12345";
    std::string port2 = "12346";

    Link link2_sender(0, 1, SENDER, ip, port1);
    std::string message = "Hello World!";
    std::cout << "Sending a Hello World!\n";
    link2_sender.send(message);
    std::cout << "Exiting...\n";
    exit(1);
  }

  std::cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}

/*int main(int argc, char **argv) {

  std::string ip = "localhost";
  std::string port1 = "12345";
  std::string port2 = "12346";

  Link link1_receiver(0, 1, RECEIVER, port1);
  Link link2_sender(0, 1, SENDER, ip, port1);

  std::string message = "Hello World!";

  int i = 0;
  // Use fork to create a new process
  pid_t pid = fork();
  if (pid != 0) {
    std::string received = link1_receiver.receive();
  }
  else {
    while (i < 1000000) {
      i++;
    }
    link2_sender.send(message);
  }
}*/

/*
int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  hello();
  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
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

  std::cout << "Doing some initialization...\n\n";

  std::cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
*/
