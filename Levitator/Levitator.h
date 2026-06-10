#pragma once
#include <AsierInho_V2.h>
#include <stdio.h>
#include <stdio.h>
#include <vector>

class Levitator {
	
	int numBoards;
	float* matBoardToWorld;
	int* boardIDs;
	int numTransducers;
	bool print;

	std::vector<float> transducerPositions;
	std::vector<float> transducerNormals;
	std::vector<float> amplitudeAdjust;
	std::vector<int> mappings;
	std::vector<int> phaseDelays;
	int numDiscreteLevels;
	int num_geometries;
	int update_rate;

	std::vector<unsigned char> phases_disc;
	std::vector<unsigned char> amplitudes_disc;


public:
	AsierInho_V2::AsierInhoBoard_V2* driver;
	Levitator( int* boardIDsIn, float* matBoardToWorldIn, int numBoardsIn = 2,bool printIn=true , int update_rate_in = 10000);
	//Levitator();
	int sendMessages(float* phases, float* amplitudes, float relative_amp=1, int num_geometriesIn =1, int sleep_ms=0, bool loop=false, int num_loops=0);
	int TurnOff();
	int Disconnect();
	int init_driver();
	int sendNewDivider(unsigned int newDivider);
	int setFrameRate(int frameRate);

	int getNumTransducers() { return this->numTransducers; };
	std::vector<float> getTransducerPositions() { return this->transducerPositions; };
};