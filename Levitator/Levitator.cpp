#include "Levitator.h"
#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

void printV2(const char* str) {
	printf("%s\n", str);
}

namespace microTimer {
	static unsigned long uGetTime(unsigned long baseTime = 0) {
		timeval tv;
		gettimeofday(&tv,NULL);

		unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;

		return time_in_micros - baseTime;
	}

	// static void uWait(useconds_t waitTime) {
	// 	usleep(waitTime);
	// }

	// static void keepUpdatePeriod(unsigned long updatePeriod) {
	// 	static unsigned long prevTime = 0;
	// 	unsigned long currentTime = microTimer::uGetTime();
	// 	while (currentTime - prevTime < updatePeriod) {
	// 		currentTime = microTimer::uGetTime();
	// 	}
	// 	prevTime = currentTime;
	// }
};


Levitator::Levitator(int* boardIDsIn, float* matBoardToWorldIn, int numBoardsIn, bool printIn, int update_rate_in) {
	if (print) { printf("Connecting to board..."); };
	numBoards = numBoardsIn;
	boardIDs = (int*)malloc(sizeof(int) * numBoards);
	memcpy(boardIDs, boardIDsIn, sizeof(int) * numBoards);
	matBoardToWorld = (float*)malloc(16 * sizeof(float) * numBoards);
	memcpy(matBoardToWorld, matBoardToWorldIn, 16 * sizeof(float) * numBoards);
	numTransducers = numBoards * 256;

	print = printIn;
	update_rate = update_rate_in;

	if (print) { printf("Connected to Board\n"); };

}

int Levitator::init_driver() {

	AsierInho_V2::RegisterPrintFuncs(printV2, printV2, printV2);
	AsierInho_V2::AsierInhoBoard_V2* driver = AsierInho_V2::createAsierInho();
	this->driver = driver;

	if (!driver->connect(numBoards, boardIDs, matBoardToWorld))
		printf("Failed to connect to board. \n");
	
	phases_disc.assign(256 * numBoards, 0);
	amplitudes_disc.assign(256 * numBoards, 0);

	//Read parameters to be used for the solver
	transducerPositions.assign(numTransducers * 3, 0);
	transducerNormals.assign(numTransducers * 3, 0);
	amplitudeAdjust.assign(numTransducers, 0);
	mappings.assign(numTransducers, 0);
	phaseDelays.assign(numTransducers, 0);
	driver->readParameters(transducerPositions.data(), transducerNormals.data(), mappings.data(), phaseDelays.data(), amplitudeAdjust.data(), &numDiscreteLevels);

	// if (numBoards > 1) {
	// 	bool x = disc->connect(AsierInho::BensDesign, boardIDs[0], boardIDs[1]);
	// }
	// else {
	// 	bool x = disc->connect(AsierInho::BensDesign, boardIDs[0]);
	// }
	// disc->disconnect();

	//float* trans = this->getTransducerPositions(); //Print Transducers
	//printf("%d\n", this->getNumTransducers());
	//for (int i = 0; i < 3*this->getNumTransducers(); i+=3) {
	//	printf("%d, %f, %f, %f \n",i/3, trans[i], trans[i+1], trans[i+2]);
	//}

	int div = 40000 / update_rate;
	this->sendNewDivider(div);

	return 0;
}

int Levitator::setFrameRate(int frameRate) {
	update_rate = frameRate;
	int div = 40000 / update_rate;
	this->sendNewDivider(div);
	return frameRate;
}

int Levitator::sendNewDivider(unsigned int newDivider) {

	std::vector<unsigned char> dividerMessage(2 * 256 * numBoards, 0);

	for (int b = 0; b < numBoards; b++) {
		memcpy(&dividerMessage[b*512], &phases_disc[b*256], 256 * sizeof(unsigned char));
		memcpy(&dividerMessage[b*512 + 256], &amplitudes_disc[b*256], 256 * sizeof(unsigned char));
		int divider = newDivider;
		dividerMessage[512 * b + 0] |= 0x80;
		
		for (int bit = 0; bit < 8; bit++) {
			dividerMessage[512 * b + 26 + bit] |= 0x80 * (divider % 2);
			divider /= 2;
		}
		dividerMessage[512 * b + 34] |= 0x80;
	}
	driver->updateMessage(dividerMessage.data());
	return 0;

}


int Levitator::sendMessages(float* phases, float* amplitudes, float relative_amp, int num_geometriesIn, [[maybe_unused]] int sleep_ms, bool loop, int num_loops) {
	num_geometries = num_geometriesIn;
	//unsigned char phases_disc[512], amplitudes_disc[512];

	int num_geometries_per_package = 32;
	if (num_geometries < num_geometries_per_package) {
		num_geometries_per_package = num_geometries;
	}

	// See Bk2. Pg 70
	int number_of_packages = num_geometries / num_geometries_per_package;
	if (number_of_packages * num_geometries_per_package < num_geometries) {
		number_of_packages++;
	}

	std::vector<unsigned char> messages;
	std::vector<unsigned char> initMessage;
	messages.reserve(2 * num_geometries * (256 * numBoards));
	initMessage.reserve(2 * (256 * numBoards));

	int geometry = 0;
	for (int p = 0; p < number_of_packages; p++) { //iterate over the packages to send at once
		int package_starting_index = p * num_geometries_per_package * 512 * numBoards;
		int num_geometries_in_this_package = std::min(num_geometries_per_package, num_geometries - geometry);
		for (int g = 0; g < num_geometries_in_this_package; g++) { //
			driver->discretizePhases(&(phases[geometry * numTransducers]), phases_disc.data());
			if (amplitudes) {
				driver->discretizeAmplitudes(&(amplitudes[geometry * numTransducers]), amplitudes_disc.data());
				driver->correctPhasesShift(phases_disc.data(), amplitudes_disc.data());
			}
			else {
				unsigned char disc_amp = driver->_discretizeAmplitude(relative_amp);
				amplitudes_disc.assign(numTransducers, disc_amp);
			}
			size_t len_per_board = 512 * num_geometries_in_this_package;
			for(int board = 0; board < numBoards; board++){
				size_t message_start_index = package_starting_index + board * len_per_board + 512 * g;
				memcpy(&messages[message_start_index], &phases_disc[256 * board], (256 * sizeof(unsigned char)));
				memcpy(&messages[message_start_index + 256], &amplitudes_disc[256 * board], (256 * sizeof(unsigned char)));
				messages[message_start_index] |= 0x80;
			}

			if (p == 0 && g == 0) { // Added by Ryuji
				for(int b = 0; b < numBoards; b++){
					memcpy(&initMessage[512 * b], &phases_disc[256 * b], (256 * sizeof(unsigned char)));
					memcpy(&initMessage[512 * b + 256], &amplitudes_disc[256 * b], (256 * sizeof(unsigned char)));
					initMessage[512 * b] |= 0x80;
				}				
			}

			geometry++;
		}
	}
	
	driver->updateMessage(&initMessage[0]);


	// unsigned long waitingPeriod = num_geometries_per_package * (1000000 / update_rate);
	// unsigned long lastUpdate = microTimer::uGetTime();
	// unsigned long currentTime = lastUpdate;
	unsigned long start = microTimer::uGetTime();

	bool in_loop = false;
	int l = 0;
	if (num_loops > 0) { loop = true; }
	while (loop || !in_loop) {
		for (int g = 0; g < num_geometries; g += num_geometries_per_package) {
			// wait a while 
			/*do {
				currentTime = microTimer::uGetTime();
			} while (currentTime - lastUpdate < waitingPeriod);*/
			// send messages to the boards
			int numMessagesToSend = std::min(num_geometries_per_package, num_geometries - g);
			driver->updateMessages(&messages[2 * g * numTransducers], numMessagesToSend);
			// waitingPeriod = numMessagesToSend * (1000000 / update_rate);
			// get the current time
			// lastUpdate = microTimer::uGetTime();
		}
		in_loop = true;
		l++;
		if (l > 0 && l >= num_loops) { loop = false; }
	}
	unsigned long end = microTimer::uGetTime();
	if (print) { printf("Estimated frame rate is %f Hz\n", l*num_geometries * 1000000.f / (end - start)); }

	return 0;
}

int Levitator::TurnOff() {
	driver->turnTransducersOff();
	return 0;
}

int Levitator::Disconnect() {
	driver->turnTransducersOff();
	usleep(100*1000);
	driver->disconnect();
	delete driver;
	return 0;
}

extern "C" {
	__attribute__((visibility("default"))) void* connect_to_levitator(int* boardIDsIn, float* matBoardToWorldIn, int numBoardsIn, bool print) { 
		Levitator* lev = new Levitator(boardIDsIn, matBoardToWorldIn, numBoardsIn, print);
		
		lev->init_driver();
		return lev;
	}

	__attribute__((visibility("default"))) int send_message (void* levitator_ptr, float* phases, float* amplitudes, float relative_amp, int num_geometries, int sleep_ms, bool loop, int num_loops) {
		try
		{
			Levitator* levitator = reinterpret_cast<Levitator*>(levitator_ptr);
			return levitator->sendMessages(phases, amplitudes, relative_amp, num_geometries,sleep_ms,loop, num_loops);
		}
		catch (...)
		{
			return -1; //assuming -1 is an error condition. 
		}
	}

	__attribute__((visibility("default"))) int disconnect(void* levitator_ptr) {
		{
			try
			{
				Levitator* levitator = reinterpret_cast<Levitator*>(levitator_ptr);
				int ret = levitator->Disconnect();
				delete levitator;
				return ret;
			}
			catch (...)
			{
				return -1; //assuming -1 is an error condition. 
			}
		}
	}

	__attribute__((visibility("default"))) int turn_off(void* levitator_ptr) {
		{
			try
			{
				Levitator* levitator = reinterpret_cast<Levitator*>(levitator_ptr);
				return levitator->TurnOff();
			}
			catch (...)
			{
				return -1; //assuming -1 is an error condition. 
			}
		}
	}

	__attribute__((visibility("default"))) int set_new_frame_rate(void* levitator_ptr, int frame_rate) {
		{
			try
			{
				Levitator* levitator = reinterpret_cast<Levitator*>(levitator_ptr);
				return levitator->setFrameRate(frame_rate);
			}
			catch (...)
			{
				return -1; //assuming -1 is an error condition. 
			}
		}
	}
	

}