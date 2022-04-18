/*
 * ipahorde.cpp
 *
 *  Created on: Mar 20, 2017
 *      Author: balyo
 */

#include "ipasir.h"
//#include "mympi.h"
#include "HordeLib.h"
#include "utilities/Logger.h"
#include "utilities/DebugUtils.h"


const char * ipasir_signature () {
	return "HordeSat";
}

HordeLib* hlib;
int* litBuffer;
int* assumpBuffer;
int* resBuffer;
int* valFailBuffer;
int varsCount = 0;
int mpi_rank,mpi_size;

const int BUFFER_SIZE = 1500;
const int MSG_LITS = 10;
const int MSG_ASSUMPTIONS = 20;
const int MSG_SOLVE = 30;
const int MSG_FINISH = 40;

void ensureValFailBuffer() {
	if (valFailBuffer == NULL) {
		valFailBuffer = (int*)malloc((varsCount)*sizeof(int));
		valFailBuffer[0] = varsCount;
	}
	if (valFailBuffer[0] != varsCount) {
		valFailBuffer = (int*)realloc(valFailBuffer, (varsCount)*sizeof(int));
		valFailBuffer[0] = varsCount;
	}
}

void bcastBuffer(int* buffer) {
	MPI_Bcast(buffer, BUFFER_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
	buffer[1] = 0;
}

void slaveSolverLoop() {
	int* buffer = (int*)malloc(BUFFER_SIZE*sizeof(int));
	bool continueLoop = true;
	while (continueLoop) {
		MPI_Bcast(buffer, BUFFER_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
		int message = buffer[0];
		switch (message) {
		case MSG_LITS:
			for (int i = 0; i < buffer[1]; i++) {
				hlib->addLit(buffer[2+i]);
			}
			break;
		case MSG_SOLVE:
		case MSG_ASSUMPTIONS:
			for (int i = 0; i < buffer[1]; i++) {
				hlib->assume(buffer[2+i]);
			}
			if (message == MSG_SOLVE) {
				int res = hlib->solve();
				MPI_Gather(&res, 1, MPI_INT, NULL, 1, MPI_INT, 0, MPI_COMM_WORLD);
				int solvedBy;
				MPI_Bcast(&solvedBy, 1, MPI_INT, 0, MPI_COMM_WORLD);
				if (solvedBy == mpi_rank) {
					// send the truth values or failed lits
					ensureValFailBuffer();
					for (int i = 1; i < varsCount; i++) {
						valFailBuffer[i] = res == 10 ? hlib->value(i) : hlib->failed(i);
					}
					MPI_Send(valFailBuffer, varsCount, MPI_INT, 0,0, MPI_COMM_WORLD);
				}
			}
			break;
		case MSG_FINISH:
			continueLoop = false;
			break;
		}
	}
	free(buffer);
}

void freeResources() {
	free(litBuffer);
	free(assumpBuffer);
	if (valFailBuffer != NULL) free(valFailBuffer);
	free(resBuffer);
	delete hlib;
}

void ipasir_setup(int argc, char** argv) {
	MPI_Init(&argc, &argv);
	hlib = new HordeLib(argc, argv);
	hlib->params.setParam("i","50");
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

	// buffer[0] = message type, buffer[1] = number of elements, buffer[2...] elements
	litBuffer = (int*)malloc(BUFFER_SIZE*sizeof(int));
	litBuffer[0] = MSG_LITS;
	litBuffer[1] = 0;
	// buffer[0] = message type, buffer[1] = number of elements, buffer[2...] elements
	assumpBuffer = (int*)malloc(BUFFER_SIZE*sizeof(int));
	assumpBuffer[0] = MSG_ASSUMPTIONS;
	assumpBuffer[1] = 0;

	valFailBuffer = NULL;
	resBuffer = (int*)malloc(mpi_size*sizeof(int));


	if (mpi_rank != 0) {
		slaveSolverLoop();
		freeResources();
		MPI_Finalize();
		exit(0);
	}
}


void ipasir_finalize() {
	assumpBuffer[0] = MSG_FINISH;
	bcastBuffer(assumpBuffer);
	freeResources();
	MPI_Finalize();
}

void * ipasir_init () {
	// replaced by ipasir_setup
	return NULL;
}

void ipasir_release (void * solver) {
	// replaced by ipasir_finalize
}

void ipasir_add (void * solver, int lit_or_zero) {
	hlib->addLit(lit_or_zero);
	if (litBuffer[1] + 2 == BUFFER_SIZE) {
		bcastBuffer(litBuffer);
	}
	litBuffer[litBuffer[1]+2] = lit_or_zero;
	litBuffer[1]++;
	if (abs(lit_or_zero) >= varsCount) {
		varsCount = 1 + abs(lit_or_zero);
	}
}

void ipasir_assume (void * solver, int lit) {
	hlib->assume(lit);
	if (assumpBuffer[1] + 2 == BUFFER_SIZE) {
		bcastBuffer(assumpBuffer);
	}
	assumpBuffer[assumpBuffer[1]+2] = lit;
	assumpBuffer[1]++;
}

int ipasir_solve (void * solver) {
	// bcast remaining literals
	bcastBuffer(litBuffer);
	// bcast remaining assumptions, piggibacking the solve command
	assumpBuffer[0] = MSG_SOLVE;
	bcastBuffer(assumpBuffer);
	assumpBuffer[0] = MSG_ASSUMPTIONS;

	int res = hlib->solve();
	MPI_Gather(&res, 1, MPI_INT, resBuffer, 1, MPI_INT, 0, MPI_COMM_WORLD);
	int solvedBy = -1;
	for (int i = 0; i < mpi_size; i++) {
		log(0, "solver %i reports %d\n", i, resBuffer[i]);
		if (resBuffer[i] != 0) {
			if (solvedBy == -1) {
				solvedBy = i;
				res = resBuffer[i];
			}
			//break;
		}
	}
	//log(0, "getting result %d from node %d\n", res, solvedBy);
	MPI_Bcast(&solvedBy, 1, MPI_INT, 0, MPI_COMM_WORLD);
	ensureValFailBuffer();
	if (solvedBy != 0) {
		MPI_Recv(valFailBuffer, varsCount, MPI_INT, solvedBy, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	} else {
		for (int i = 1; i < varsCount; i++) {
			valFailBuffer[i] = res == 10 ? hlib->value(i) : hlib->failed(i);
		}
	}

	return res;
}

int ipasir_val (void * solver, int lit) {
	return lit > 0 ? valFailBuffer[lit] : -valFailBuffer[-lit];
}

int ipasir_failed (void * solver, int lit) {
	return lit > 0 ? valFailBuffer[lit] : -valFailBuffer[-lit];
}

void ipasir_set_terminate (void * solver, void * state, int (*terminate)(void * state)) {
	//TODO
}

void ipasir_set_learn (void * solver, void * state, int max_length, void (*learn)(void * state, int * clause)) {
	//TODO
}



