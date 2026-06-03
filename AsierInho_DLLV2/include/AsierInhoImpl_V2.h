#ifndef _ASIERINHO_IMPLEMENTATION
#define _ASIERINHO_IMPLEMENTATION
#include "AsierInho_V2.h"
#include "AsierInho_V2_Prerequisites.h"
#include "BoardWorkerThread.h"
#include <math.h>
#include <memory>
#include <stdio.h>
#include <vector>
#define _USE_MATH_DEFINES
#include <ftd2xx.h>

using namespace AsierInho_V2;

class AsierInhoImpl_V2 : public AsierInhoBoard_V2 {
  // ATTRIBUTES:
public:
  static const size_t messageSize = 512;

protected:
  std::vector<int> boardIDs; // botBoardID, topBoardID;
  int numBoards;             // = 2;
  int numDiscreteLevels;
  int maxNumMessagesToSend;
  // Callibration parameters...
  int *transducerIds;         // [256 * numBoards];
  int *phaseAdjust;           // [256 * numBoards];
  float *transducerPositions; //[3*256 * numBoards]
  float *transducerNormals;   //[3*256 * numBoards]
  float *amplitudeAdjust;
  std::vector<char *> serialNumbers; // bottomSerialNumber, *topSerialNumber;
  enum AsierInhoState { INIT = 0, CONNECTED };
  int status;

  // Worker threads: They wait untill notified to update phases, and send a
  // condition variable notification back when the update is complete.
  std::vector<std::unique_ptr<worker_threadData>> boardWorkerData;

public:
  AsierInhoImpl_V2();
  ~AsierInhoImpl_V2();
  virtual bool connect(int bottomBoardID, int topBoardID,
                       int maxNumMessagesToSend);
  virtual bool connect(int numBoards, int *boardIDs, float *matBoardToWorld4x4,
                       int maxNumMessagesToSend);
  virtual void readParameters(float *transducerPositions,
                              float *transducerNormals, int *transducerIds,
                              int *phaseAdjust, float *amplitudeAdjust,
                              int *numDiscreteLevels);
  virtual size_t totalTransducers() { return totalBoards() * 256; };
  virtual size_t totalBoards() { return numBoards; }
  virtual void numTransducersPerBoard(size_t *out_TransPerBoard) {
    for (int b = 0; b < numBoards; b++)
      out_TransPerBoard[b] = 256;
  }
  virtual void updateBoardPositions(float *matBoardToWorld4x4);
  virtual void updateMessagePerBoard(unsigned char **message);
  virtual void updateMessage(unsigned char *messages);
  virtual void updateMessages(unsigned char *messages, int numMessagesToSend);
  virtual void turnTransducersOn();
  virtual void turnTransducersOff();
  virtual void disconnect();
#ifdef _TIME_PROFILING
  virtual void _profileTimes();
#endif

protected:
  void readBoardParameters(int boardId, float *matToWorld,
                           float *transducerPositions, float *transducerNormals,
                           int *transducerIds, int *phaseAdjust,
                           float *amplitudeAdjust, int *numDiscreteLevels);
};

#endif