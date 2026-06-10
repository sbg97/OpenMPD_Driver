#include "BoardWorkerThread.h"

#include "AsierInhoImpl_V2.h"

static char consoleLineBuffer[512];

/**
        Method run by each of the wroker threads
*/
void worker_threadData::worker_BoardUpdater() {
  while (running) {
    // lock, then unlock the mutex, then relock it again once either
    // `numMessagesToSend` is not zero, or we are trying to disconnect
    std::unique_lock<std::mutex> lock(currentlyDoingWork);
    threadBlocker.wait(lock, [this] { return numMessagesToSend || !running; });
    if (!running) {
      break;
    }
    // Send update:
    sendUpdate(dataStream.data(), numMessagesToSend);
#ifdef _TIME_PROFILING
    struct timeval curTime;
    curUpdate = (curUpdate + 1) % UPS;
    gettimeofday(&curTime, 0x0);
    timestamps[curUpdate] = computeTimeElapsed(referenceTime, curTime);
#endif
    // Send notification signal
    numMessagesToSend = 0;
    threadBlocker.notify_all();
  }
  AsierInho_V2::printMessage("AsierInho Worker finished!\n");
}

worker_threadData::worker_threadData(std::string boardSerialNumber)
    : boardSN(boardSerialNumber),
      workerThread(&worker_threadData::worker_BoardUpdater, this) {
  running = initFTDevice(false);
  if (!running) {
    if (boardSerialNumber.length() > 400) {
      boardSerialNumber = boardSerialNumber.substr(0, 399);
    }
    sprintf(consoleLineBuffer,
            "initFTDevice() for boardSN \"%s\" was not successful\n",
            boardSerialNumber.c_str());
    printWarning(consoleLineBuffer);
  }
}

worker_threadData::~worker_threadData() {
  // wait until the thread is done doing what it's in the middle of doing,
  // then tell it to turn off
  {
    std::unique_lock<std::mutex> lock(currentlyDoingWork);
    running = false;
  }
  threadBlocker.notify_all();

  workerThread.join();
  FT_Close(board);
}

// To achieve 10kHz, I need to use the FTD2XX driver. USBs are not recognised as
// a COM port any more...
bool worker_threadData::initFTDevice(bool syncMode) {
  FT_STATUS status;
  status = FT_OpenEx(const_cast<char*>(boardSN.c_str()),
                     FT_OPEN_BY_SERIAL_NUMBER, &board);
  if (status != FT_OK) {
    if (status == FT_INVALID_HANDLE)
      sprintf(
          consoleLineBuffer,
          "Could not open USB port, status not ok (%d - invalid handle...)\n",
          status);
    if (status == FT_DEVICE_NOT_FOUND)
      sprintf(consoleLineBuffer,
              "Could not open USB port, open status not ok (%d - device "
              "not found...)\n",
              status);
    if (status == FT_DEVICE_NOT_OPENED)
      sprintf(consoleLineBuffer,
              "Could not open USB port, open status not ok (%d - device "
              "not opened...)\n",
              status);
    AsierInho_V2::printWarning(consoleLineBuffer);
    return false;
  }
  AsierInho_V2::printMessage("USB ports open\n");
  status = FT_ResetDevice(board);
  if (status != FT_OK) {
    sprintf(consoleLineBuffer, "Could not reset USB ports. Reset status %d\n",
            status);
    AsierInho_V2::printWarning(consoleLineBuffer);
    return false;
  }

  // set the transfer mode as Syncronsou mode
  UCHAR Mask = 0xFF;  // Set data bus to outputs
  UCHAR mode_rst = 0;
  FT_SetBitMode(board, Mask, mode_rst);  // reset MPSSE
  if (syncMode) {
    UCHAR mode_sync = 0x40;
    FT_SetBitMode(board, Mask,
                  mode_sync);  // configure FT2232H into Sync FIFO mode
  }

  // set the latency timer and USB parameters
  FT_SetLatencyTimer(board, 2);
  FT_SetUSBParameters(board, 0x10000, 0x10000);
  FT_Purge(board, FT_PURGE_RX | FT_PURGE_TX);
  AsierInho_V2::printMessage("USB ports setup\n");
  return true;
}

/*
Sends a command to the board to load new phases and amplitudes for its 256
transducers.
*/
void worker_threadData::sendUpdate(unsigned char* stream, size_t numMessages) {
  FT_STATUS ftStatus;
  DWORD dataWritten;
  // 4. Send!
  ftStatus = FT_Write(board, stream,
                      (DWORD)numMessages * AsierInhoImpl_V2::messageSize,
                      &dataWritten);
  if (ftStatus != FT_OK) {
    sprintf(consoleLineBuffer, "worker_threadData::sendUpdate error %d\n",
            ftStatus);
    printWarning(consoleLineBuffer);
  }
}