#include "SerialNumberList.h"
#include <stdio.h>
#include <sstream>
int firstIDToGenerate = 1;
int lastIDToGenerate = 25; //Change to fit number of boards in the .h file.

/**
	This program reads the file describing our boards' configuration (SerialNumberList.h)
	and generates the configuration files for the boards. 
	Please note it will re-generate all files in the sequence (from firstIDToGenerate to lastIDToGenerate (included).
*/

/**
	Computes the 3D position of a transducer, given its index in the board. 
	The board is assumed to rest at Z=0, and is composed of boardSize=32x16 number of transducers
	Boards are alocated "Side by Side" (i.e. first row of bottom board, followed by first row of top board).
	The system of reference of the coordinate system is assumed to be at the centre of the bottom board. 
	The pitch (separation between transducers) is defined in variable pitch
	pos_out returns the resulting position
	NOTE: This method is retained for backwards compatibility, as GS_PAT uses it to determine location of 
	transducers. We will be changing this in the future and only the version above will be used.
*/
static void computeTransducerPos(int x, int y, float pitch, float pos_out[3]) {
	pos_out[0] = (x - 7.5f)*pitch;
	pos_out[1] = (7.5f - y )*pitch;
	pos_out[2] = 0;
}

int main(void) {
	for (int boardID = firstIDToGenerate; boardID  <= lastIDToGenerate; boardID ++) {
		int indexInSerialNumberFile = boardID - 1;//Board IDs go from 1..N, but the array in which they are stored starts at 0.
		
		std::stringstream filename;
		filename<< "board_" << (boardID) << ".pat";
		FILE* file = fopen(filename.str().c_str(), "w");
		//Save hardware ID
		fprintf(file, "%s\n", FushimiSerialNumbers[indexInSerialNumberFile ]);
		//Save numtransducers
		fprintf(file, "256\n");
		//Save numdiscrete levels
		fprintf(file, "128\n");
		//Save transducers positions:
		for(int x=0; x<16; x++)
			for (int y = 0; y < 16; y++) {
				int t_id[] = { x,y };
				float pos[3];
				computeTransducerPos(x, y, 0.0105f, pos);
				fprintf(file, "(%f, %f, %f),", pos[0], pos[1], pos[2]);
			}
		fprintf(file, "\n");
		//Save transducer -> PIN mapping:
		for (int i = 0; i < 256; i++)
			fprintf(file, "%d,", BensMapping[i]);
		fprintf(file, "\n");
		//Save phase corrections
		for (int i = 0; i < 256; i++)
			fprintf(file, "%d,", 0);
		fprintf(file, "\n");
		//Save amplitude corrections
		for (int i = 0; i < 256; i++)
			fprintf(file, "%f,", 1.0f);
		fprintf(file, "\n");
		fclose(file);
	}
}