#include "AsierInhoImpl_V2.h"
#include "ParseBoardConfig.h"
#include <cstring>
#include <memory>
#include <mutex>
#include <unistd.h>

void mySleep(int ms) {
#ifdef _WIN32
  Sleep(ms)
#else
  usleep(ms * 1000);
#endif
}

static char consoleLineBuffer[512];

#ifdef _TIME_PROFILING
void AsierInhoImpl_V2::_profileTimes() {
  // Lock all boards:
  for (int b = 0; b < boardWorkerData.size(); b++) {
    pthread_mutex_lock(&(boardWorkerData[b]->send_signal));
  }
  // Write all time profiles:
  for (int b = 0; b < boardWorkerData.size(); b++) {
    FILE *boardFile;
    fopen_s(&boardFile, boardWorkerData[b]->boardSN, "a");
    for (int u = 0; u < boardWorkerData[b]->UPS; u++) {
      int update =
          (u + boardWorkerData[b]->curUpdate) % boardWorkerData[b]->UPS;
      fprintf_s(boardFile, "%d, %f\n", update,
                boardWorkerData[b]->timestamps[update]);
    }
    fclose(boardFile);
  }
  // Unlock all boards:
  for (int b = 0; b < boardWorkerData.size(); b++) {
    pthread_mutex_unlock(&(boardWorkerData[b]->send_signal));
  }
}

#endif

AsierInhoImpl_V2::AsierInhoImpl_V2() : AsierInhoBoard_V2(), status(INIT) {}

AsierInhoImpl_V2::~AsierInhoImpl_V2() {}

/**
        Connects to the board, using the board type and ID specified.
        ID corresponds to the index we labeled on each board.
        Returns true if connection was succesfull.
*/
bool AsierInhoImpl_V2::connect(int bottomBoardID, int topBoardID,
                               int maxNumMessagesToSend) {
  // Configuration data for a simple top-bottom setup.
  int boardIDs[] = {bottomBoardID, topBoardID};
  // clang-format off
  float matBoardToWorld[32] = {
      /*bottom*/
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1,
      /*top*/
      -1, 0, 0,  0,
      0,  1, 0,  0,
      0,  0, -1, 0.2388f,
      0,  0, 0,  1,
  };
  // clang-format on
  if (topBoardID != 0) // Support for sigle sided setups
    return connect(2, boardIDs, matBoardToWorld, maxNumMessagesToSend);
  else
    return connect(1, boardIDs, matBoardToWorld, maxNumMessagesToSend);
}

bool AsierInhoImpl_V2::connect(int numBoards, int *boardIDs, float *matToWorld,
                               int maxNumMessagesToSend) {
  this->numBoards = numBoards;
  this->maxNumMessagesToSend = maxNumMessagesToSend;
  transducerPositions = new float[256 * 3 * numBoards];
  transducerNormals = new float[256 * 3 * numBoards];
  amplitudeAdjust = new float[256 * numBoards];
  phaseAdjust = new int[256 * numBoards];
  transducerIds = new int[256 * numBoards];
  int *numDiscreteLevels = new int[numBoards];
  // We will set the resolution to the MINIMUM of all board involved
  this->numDiscreteLevels = 256;
  // 2. Read per-board adjustment parameters and safe them in each worker thread
  // data:
#ifdef _TIME_PROFILING
  struct timeval reference;
  gettimeofday(&reference, 0x0);
#endif
  for (int b = 0; b < numBoards; b++) {
    this->boardIDs.push_back(boardIDs[b]);
    BoardConfig boardConfig = ParseBoardConfig::readParameters(boardIDs[b]);
    boardWorkerData.push_back(
        std::make_unique<worker_threadData>(boardConfig.hardwareID));
    boardWorkerData.back()->dataStream.reserve(messageSize *
                                               maxNumMessagesToSend);
    readBoardParameters(boardIDs[b], &(matToWorld[16 * b]),
                        &(transducerPositions[3 * 256 * b]),
                        &(transducerNormals[3 * 256 * b]),
                        &(transducerIds[256 * b]), &(phaseAdjust[256 * b]),
                        &(amplitudeAdjust[256 * b]), &numDiscreteLevels[b]);
    if (numDiscreteLevels[b] < this->numDiscreteLevels)
      this->numDiscreteLevels = numDiscreteLevels[b];
#ifdef _TIME_PROFILING // DEBUG: Add time profiling fields:
    boardWorkerData[b]->referenceTime = reference;
    boardWorkerData[b]->curUpdate = 0;
#endif // END DEBUG
  }
  delete[] numDiscreteLevels;

  // Adjust from (local) transducer IDs [0..255] to global positions in the
  // message
  {
    for (int b = 1; b < numBoards; b++)
      for (int t = 0; t < 256; t++)
        this->transducerIds[t + b * 256] += b * 256;
  }
  // 3. Connect to each of the boards:
  if (status != AsierInhoState::INIT) {
    AsierInho_V2::printWarning("AsierInho_V2 is already connected\n");
    return false;
  }
  bool allConnected = true;
  for (auto &boardworkerdata : boardWorkerData) {
    allConnected &= boardworkerdata->running;
  }

  if (!allConnected) {
    boardWorkerData.clear();
    AsierInho_V2::printWarning("AsierInho_V2 connection failed\n");
    return false;
  }
  status = AsierInhoState::CONNECTED;
  // for (int b = 0; b < numBoards; b++) {
  //   pthread_attr_t attrs;
  //   pthread_attr_init(&attrs);
  //   pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
  //   pthread_t thread;
  //   boardWorkerThreads.push_back(thread);
  //   pthread_create(&boardWorkerThreads[b], &attrs, &worker_BoardUpdater,
  //                  ((void *)boardWorkerData[b]));
  // }
  mySleep(100);
  AsierInho_V2::printMessage("AsierInho_V2 connected succesfully\n");
  return true;
}

void AsierInhoImpl_V2::readParameters(float *transducerPositions,
                                      float *transducerNormals,
                                      int *transducerIds, int *phaseAdjust,
                                      float *amplitudeAdjust,
                                      int *numDiscreteLevels) {
  memcpy(transducerPositions, this->transducerPositions,
         256 * numBoards * 3 * sizeof(float));
  memcpy(transducerNormals, this->transducerNormals,
         256 * numBoards * 3 * sizeof(float));
  memcpy(transducerIds, this->transducerIds, 256 * numBoards * sizeof(int));
  memcpy(phaseAdjust, this->phaseAdjust, 256 * numBoards * sizeof(int));
  memcpy(amplitudeAdjust, this->amplitudeAdjust,
         256 * numBoards * sizeof(float));
  *numDiscreteLevels = this->numDiscreteLevels;
}

void AsierInhoImpl_V2::updateBoardPositions(float *matBoardToWorld4x4) {
  if (status != AsierInhoState::CONNECTED)
    return;
  for (int b = 0; b < numBoards; b++) {
    // Wait until worker thread isn't doing anything
    std::unique_lock<std::mutex> lock(boardWorkerData[b]->currentlyDoingWork);
    // Update configuration (we simlpy re-read the config file, applying the
    // new matrix).
    int tmp; // we do not need to reload the numDiscreteLevels... we will
             // store it here and ignore this return value
    readBoardParameters(boardIDs[b], &(matBoardToWorld4x4[16 * b]),
                        &(transducerPositions[3 * 256 * b]),
                        &(transducerNormals[3 * 256 * b]),
                        &(transducerIds[256 * b]), &(phaseAdjust[256 * b]),
                        &(amplitudeAdjust[256 * b]), &tmp);
  }
}

void AsierInhoImpl_V2::readBoardParameters(int boardId, float *matToWorld,
                                           float *transducerPositions,
                                           float *transducerNormals,
                                           int *transducerIds, int *phaseAdjust,
                                           float *amplitudeAdjust,
                                           int *numDiscreteLevels) {
  BoardConfig boardConfig = ParseBoardConfig::readParameters(boardId);
  // 0. Num discrete levels
  *numDiscreteLevels = boardConfig.numDiscreteLevels;
  // 1. Write transducer positions (multiply local pos by matrix):
  for (int t = 0; t < 256; t++) {
    float *pLocal = &(boardConfig.positions[3 * t]);
    float posWorld[3] = {
        pLocal[0] * matToWorld[0] + pLocal[1] * matToWorld[1] +
            pLocal[2] * matToWorld[2] + matToWorld[3],
        pLocal[0] * matToWorld[4] + pLocal[1] * matToWorld[5] +
            pLocal[2] * matToWorld[6] + matToWorld[7],
        pLocal[0] * matToWorld[8] + pLocal[1] * matToWorld[9] +
            pLocal[2] * matToWorld[10] + matToWorld[11],
    };
    memcpy(&(transducerPositions[3 * t]), posWorld, 3 * sizeof(float));
    float nLocal[3] = {0, 0, 1.f};
    float normWorld[3] = {
        nLocal[0] * matToWorld[0] + nLocal[1] * matToWorld[1] +
            nLocal[2] * matToWorld[2],
        nLocal[0] * matToWorld[4] + nLocal[1] * matToWorld[5] +
            nLocal[2] * matToWorld[6],
        nLocal[0] * matToWorld[8] + nLocal[1] * matToWorld[9] +
            nLocal[2] * matToWorld[10],
    };
    memcpy(&(transducerNormals[3 * t]), normWorld, 3 * sizeof(float));
  }
  // 2. Write transducer IDs:
  memcpy(transducerIds, boardConfig.pinMapping, 256 * sizeof(int));
  // 3. Write Phase adjustments:
  memcpy(phaseAdjust, boardConfig.phaseAdjust, 256 * sizeof(int));
  // 4. Write Amplitude adjustments:
  memcpy(amplitudeAdjust, boardConfig.amplitudeAdjust, 256 * sizeof(float));
}

/*
Sends a message to each connected board.
Message is an array of messages, each one getting sent to a different board.
*/
void AsierInhoImpl_V2::updateMessagePerBoard(unsigned char **message) {
  // Doesn't work if there are no worker threads
  if (status != AsierInhoState::CONNECTED)
    return;

  for (int b = 0; b < numBoards; b++) {
    // Wait until thread is not currently doing work and has no work to do
    std::unique_lock<std::mutex> lock(boardWorkerData[b]->currentlyDoingWork);
    boardWorkerData[b]->threadBlocker.wait(
        lock, [this, b] { return boardWorkerData[b]->numMessagesToSend == 0; });

    // Update the buffers:
    boardWorkerData[b]->dataStream.assign(message[b], message[b] + messageSize);
    boardWorkerData[b]->numMessagesToSend = 1;
  }
  // Send signals to notify worker threads to update phases
  for (int b = 0; b < numBoards; b++)
    boardWorkerData[b]->threadBlocker.notify_all();
}

/*
Sends a message to each connected board.
`message` is a very long message that gets split up then sent to each board.
`message` gets split every `messageSize` bytes.
*/
void AsierInhoImpl_V2::updateMessage(unsigned char *message) {
  // Doesn't work if there are no worker threads
  if (status != AsierInhoState::CONNECTED)
    return;

  for (int b = 0; b < numBoards; b++) {
    // Wait until thread is not currently doing work and has no work to do
    std::unique_lock<std::mutex> lock(boardWorkerData[b]->currentlyDoingWork);
    boardWorkerData[b]->threadBlocker.wait(
        lock, [this, b] { return boardWorkerData[b]->numMessagesToSend == 0; });

    // Update the buffer:
    boardWorkerData[b]->dataStream.assign(message + (messageSize * b),
                                          message + (messageSize * (b + 1)));
    boardWorkerData[b]->numMessagesToSend = 1;
  }

  // Send signals to notify worker threads to update phases
  for (int b = 0; b < numBoards; b++)
    boardWorkerData[b]->threadBlocker.notify_all();
}

/*
Sends `numMessagesToSend` messages to each connected board.
`message` is a very long message that gets split up then sent to each board.
`message` gets split every `messageSize * numMessagesToSend` bytes.
*/
void AsierInhoImpl_V2::updateMessages(unsigned char *message,
                                      int numMessagesToSend) {
  // Doesn't work if there are no worker threads
  if (status != AsierInhoState::CONNECTED)
    return;

  if (numMessagesToSend > maxNumMessagesToSend || numMessagesToSend < 1) {
    AsierInho_V2::printWarning(
        "AsierInho: The driver cannot send the requested number of messages. "
        "Command Ignored.");
    return;
  }

  for (int b = 0; b < numBoards; b++) {
    // Wait until thread is not currently doing work and has no work to do
    std::unique_lock<std::mutex> lock(boardWorkerData[b]->currentlyDoingWork);
    boardWorkerData[b]->threadBlocker.wait(
        lock, [this, b] { return boardWorkerData[b]->numMessagesToSend == 0; });

    // Update the buffer:
    boardWorkerData[b]->dataStream.assign(
        message + (messageSize * numMessagesToSend * b),
        message + (messageSize * numMessagesToSend * (b + 1)));
    boardWorkerData[b]->numMessagesToSend = numMessagesToSend;
  }

  // Send signals to notify worker threads to update phases
  for (int b = 0; b < numBoards; b++)
    boardWorkerData[b]->threadBlocker.notify_all();
}

/**
This method turns the transducers off (so that the board does not heat up/die
misserably) The board is still connected, so it can later be used again (e.g.
create new traps)
*/
void AsierInhoImpl_V2::turnTransducersOff() {
  // 0. Check status
  if (status != AsierInhoState::CONNECTED)
    return;

  // 1. Set all phases and amplitudes to "0"== OFF and send!
  unsigned char *message = new unsigned char[512 * numBoards];
  memset(message, 0,
         512 * numBoards *
             sizeof(unsigned char)); // Set all phases and amplitudes.
  for (int b = 0; b < numBoards; b++) {
    message[0 + 512 * b] = 0 + 128; // Set flag indicating new update (>128)
    message[25 + 512 * b] = 128; // Set flag indicating to change the LED colour
                                 // (turn off in this case)
  }
  // 2. Send it:
  updateMessage(message);
  delete[] message;
}

void AsierInhoImpl_V2::turnTransducersOn() {
  // 0. Check status
  if (status != AsierInhoState::CONNECTED)
    return;

  // 1. Set all phases and amplitudes to "0"== OFF and send!
  unsigned char *message = new unsigned char[512 * numBoards];
  memset(message, 64,
         512 * numBoards *
             sizeof(unsigned char)); // Set all phases and amplitudes.
  for (int b = 0; b < numBoards; b++) {
    message[0 + 512 * b] = 64 + 128;  // Set flag indicating new update (>128)
    message[25 + 512 * b] = 64 + 128; // Set flag indicating to change the LED
                                      // colour (turn off in this case)
  }
  // 2. Send it:
  updateMessage(message);
  delete[] message;
}

/**
        This method disconnects the board, closing COM ports.
*/
void AsierInhoImpl_V2::disconnect() {
  if (status != AsierInhoState::CONNECTED)
    return;

  boardWorkerData.clear();
  status = AsierInhoState::INIT;
}
