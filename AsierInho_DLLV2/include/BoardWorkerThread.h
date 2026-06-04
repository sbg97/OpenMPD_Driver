#pragma once

#include <ftd2xx.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct worker_threadData {
  // Port to write to
  FT_HANDLE board;
  std::string boardSN;
  bool running = true;
  std::mutex currentlyDoingWork;
  std::condition_variable threadBlocker;
  // We read the phases/amplitudes to send from this buffer.
  std::vector<unsigned char> dataStream;
  int numMessagesToSend = 0;
  // workerThread must be at the bottom so it gets initialized after running
  // gets set to true and numMessagesToSend gets set to 0
  std::thread workerThread;
#ifdef _TIME_PROFILING // DEBUG: Add time profiling fields:
  static const int UPS = 8000;
  int curUpdate;
  float timestamps[UPS];
  struct timeval referenceTime;
#endif
  worker_threadData(std::string boardSerialNumber);
  ~worker_threadData();
  // mutexes don't like being moved/copied, so we can't move/copy this object
  worker_threadData(worker_threadData &&other) = delete;
  worker_threadData(const worker_threadData &) = delete;
  worker_threadData &operator=(const worker_threadData &) = delete;
  bool initFTDevice(bool syncMode);
  void worker_BoardUpdater();
  void sendUpdate(unsigned char *stream, size_t numMessages);
};