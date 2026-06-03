#pragma once

#include <ftd2xx.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct worker_threadData {
  FT_HANDLE board; // Port to write to
  std::string boardSN;
  bool running = true;
  std::mutex currentlyDoingWork;
  std::condition_variable threadBlocker;
  std::vector<unsigned char>
      dataStream; // We read the phasemaphores instead of mutexes?ses/amplitudes
                  // to send from this buffer.
  int numMessagesToSend;
  std::thread workerThread;
#ifdef _TIME_PROFILING // DEBUG: Add time profiling fields:
  static const int UPS = 8000;
  int curUpdate;
  float timestamps[UPS];
  struct timeval referenceTime;
#endif
  worker_threadData(std::string boardSerialNumber);
  ~worker_threadData();
  worker_threadData(worker_threadData &&other) = delete;
  worker_threadData(const worker_threadData &) = delete;
  worker_threadData &operator=(const worker_threadData &) = delete;
  bool initFTDevice(bool syncMode);
  void worker_BoardUpdater();
};