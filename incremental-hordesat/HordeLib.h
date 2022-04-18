/*
 * HordeLib.h
 *
 *  Created on: Mar 24, 2017
 *      Author: balyo
 */

#ifndef HORDELIB_H_
#define HORDELIB_H_

#include "utilities/ParameterProcessor.h"
#include "utilities/Threading.h"
#include "solvers/MiniSat.h"
#include "solvers/Lingeling.h"
#include "solvers/DepQBF.h"
#include "sharing/AllToAllSharingManager.h"
#include "sharing/LogSharingManager.h"
#include "sharing/AsyncRumorSharingManager.h"


#include <vector>
#include <set>

using namespace std;

class HordeLib {
private:
	int mpi_size;
	int mpi_rank;
	Thread** solverThreads;
	bool(*endingFunction)(int,int,bool);
	size_t sleepInt;
	int solversCount;
	bool solvingDoneLocal;
	Mutex interruptLock;
	SharingManagerInterface* sharingManager;
	vector<PortfolioSolverInterface*> solvers;

	SatResult finalResult;
	vector<int> assumptions;
	vector<int> truthValues;
	set<int> failedAssumptions;


	void stopAllSolvers();
	// diversifications
	void sparseDiversification(int mpi_size, int mpi_rank);
	void randomDiversification(unsigned int seed);
	void sparseRandomDiversification(unsigned int seed, int mpi_size);
	void nativeDiversification(int mpi_rank, int mpi_size);
	void binValueDiversification(int mpi_size, int mpi_rank);


public:
	friend void* solverRunningThread(void*);

	// settings
	ParameterProcessor params;

	// methods
	HordeLib(int argc, char** argv);
	virtual ~HordeLib();

	// dismspec solving
	bool readDimspecFile(const char* filename);
	int solveDimspec();


	// sat/qbf solving
	bool readFormula(const char* filename);
	void addLit(int lit);
	void assume(int lit);
	int solve();
	int value(int lit);
	int failed(int lit);
	ParameterProcessor& getParams() {
		return params;
	}

};

#endif /* HORDELIB_H_ */
