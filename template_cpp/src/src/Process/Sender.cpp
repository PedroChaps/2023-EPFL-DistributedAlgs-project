//
// Created by ch4ps on 04-10-2023.
//

#include "Sender.h"
#include <iostream>
#include <signal.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#define DEBUG 1
template <class T>
void debug(T msg) {
  if (DEBUG) {
    std::cout << msg << std::endl;
  }
}

Sender::Sender(std::string ipAddress, std::string port, std::string logsPath, std::stringstream *logsBuffer, int processId, int m) : port(port), logsPath(logsPath), processId(processId), m(m) {

  logsBufferPtr = logsBuffer;

  // Creates a link for each thread
  for (int i = 0; i < N_THREADS; i++) {
    PerfectLink link(SENDER, ipAddress, port);
    links.push_back(link);
  }
}

std::queue<std::string> messageQueue;
std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> hasMessages(true);

void sendMessageThread(int threadId, std::basic_stringstream<char> *logsBufferPtr, PerfectLink link) {

  while (1) {
    std::string message;
    // Creates block for the critical section
    {
      std::unique_lock<std::mutex> lock(mtx);

      // Wait until there's a message in the queue or all messages have been sent
      cv.wait(lock, [] { return !messageQueue.empty() || !hasMessages; });

      if (messageQueue.empty()) {
        return;
      }

      // Gets the message from the queue
      message = messageQueue.front();
      messageQueue.pop();

      // Reads the number of messages sent
      int nMessagesSent = std::stoi(message.substr(message.find(' ') + 1));

      // Appends to the log variable
      (*logsBufferPtr) << "b " << nMessagesSent << " " << std::endl;
    }

    // Simulate sending the message (you can replace this with your actual sending logic)
    link.send(message);
    std::cout << "Thread " << threadId << " sent message: " << message << std::endl;
  }
}

/**
 * With the use of a PerfectLink, sends broadcasts to the destiny.
 *
 * */
void Sender::sendBroadcasts() {

  std::vector<std::thread> threads;
  for (int i = 0; i < N_THREADS; i++) {
    auto idx = static_cast<unsigned long>(i);
    threads.emplace_back(sendMessageThread, i, logsBufferPtr, links[idx]);
  }


  for (int i = 1; i <= m; i++) {

    // Build the message
    std::string message = std::to_string(processId) + " " + std::to_string(i);

    {
      std::lock_guard<std::mutex> lock(mtx);
      messageQueue.push(message);
    }
    cv.notify_one(); // Notify a waiting thread that there's a message to process
  }

  hasMessages = false;
  cv.notify_all();

  // Wait for all threads to finish
  for (std::thread& t : threads) {
    t.join();
  }


  saveLogs();
}

void Sender::saveLogs() {

  std::ofstream logFile;
  logFile.open(logsPath, std::ios_base::app);
  logFile << (*logsBufferPtr).str();

  // Clears the buffer
  (*logsBufferPtr).str("");

  logFile.close();
}