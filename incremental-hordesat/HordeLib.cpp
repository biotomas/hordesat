/*
 * HordeLib.cpp
 *
 *  Created on: Mar 24, 2017
 *      Author: balyo
 */

#include "HordeLib.h"
//============================================================================
// Name        : hordesat.cpp
// Author      : Tomas Balyo
// Version     : $Revision: 159 $
// Date        : $Date: 2017-03-24 13:01:16 +0100 (Fri, 24 Mar 2017) $
// Copyright   : copyright KIT
//============================================================================

/* TODO
 * Ideas for improvement:
 * - filter incomming clauses by some score, local glue? based on assigned lits in it?
 * - add probsat
 * - see slides for sat talk
 * - local clause database at the nodes - do not add each incoming
 * 	 clause to the blackbox, only store, maybe add later or delete
 * - using hashing distribute the work on clauses (each clause belongs
 *   one CPU that works with it - for simplification, is this applicable?
 * - more asynchronous communication. *
 *
 * Experiments:
 * - Look at all the learned clauses produced in a parallel solving, how big is the overlap? does high overlap
 *   signal that the solvers do redundant work (all do the same). Do they? Do we need explicit diversification?
 *
 * Further Ideas
 * - Blocked set solver (without UP, 1-watched literal do watch the small set, precompute point-of-no-return
 *   which is the last point a variable can be flipped (last time a blit).
 * - DPLL without unit propagation (1-watched literal), can learn clauses
 * - DPLL with Path Consistency (literal encoding SAT->CSP [Walsch 2012])
 * - measure how large are the subproblems during solving, is it worth to launch a new instance of a SAT solver for
 * 	 the subproblem? (simplified formula), cache optimization etc.
 */

#include "utilities/DebugUtils.h"
#include "utilities/mympi.h"

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include "utilities/Logger.h"
#include <sched.h>


// =========================
// end detection
// =========================
#define SOLVING_DONE -10

int litsAdded = 0;

void HordeLib::stopAllSolvers() {
	interruptLock.lock();
	solvingDoneLocal = true;
	for (int i = 0; i < solversCount; i++) {
		solvers[i]->setSolverInterrupt();
	}
	interruptLock.unlock();
}

bool getNeverEnding(int mpi_size, int mpi_rank, bool solvingDoneLocal) {
	return false;
}

bool getSingleProcessEnding(int mpi_size, int mpi_rank, bool solvingDoneLocal) {
	if (solvingDoneLocal) {
		return true;
	}
	return false;
}

// Return true if the solver should stop.
bool getGlobalEnding(int mpi_size, int mpi_rank, bool solvingDoneLocal) {
	static int* endingBuffer = NULL;
	if (endingBuffer == NULL) {
		endingBuffer = new int[mpi_size];
	}
	int sendMsg = 0;
	if (solvingDoneLocal) {
		sendMsg = SOLVING_DONE;
	}
	MPI_Allgather(&sendMsg, 1, MPI_INT, endingBuffer, 1, MPI_INT, MPI_COMM_WORLD);
	for (int r = 0; r < mpi_size; r++) {
		if (endingBuffer[r] == SOLVING_DONE) {
			return true;
		}
	}
	return false;
}

// Might lead to deadlock if used with synchronous clause exchange.
#define TAG_ENDING 99
bool getGlobalEndingAsynchronous(int mpi_size, int mpi_rank, bool solvingDoneLocal) {
	static bool first = true;
	static MPI_Request request;
	static int message;
	if (first) {
		first = false;
		MPI_Irecv(&message, 1, MPI_INT, MPI_ANY_SOURCE, TAG_ENDING, MPI_COMM_WORLD, &request);
	}
	int flag = 0;
	MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
	if (flag && message == SOLVING_DONE) {
		return true;
	}

	if (solvingDoneLocal) {
		int sendMsg = SOLVING_DONE;
		MPI_Request r = MPI_REQUEST_NULL;
		for (int i = 0; i < mpi_size; i++) {
			MPI_Isend(&sendMsg, 1, MPI_INT, i, TAG_ENDING, MPI_COMM_WORLD, &r);
		}
	}

	return false;
}

void pinThread(int solversCount) {
	static int lastCpu = 0;
	int numCores = sysconf(_SC_NPROCESSORS_ONLN);
	int localRank = 0;
	const char* lranks = getenv("OMPI_COMM_WORLD_LOCAL_RANK");
	if (lranks == NULL) {
		log(0, "WARNING: local rank was not determined.\n");
	} else {
		localRank = atoi(lranks);
	}
	int desiredCpu = lastCpu + localRank*solversCount;
	lastCpu++;
	log(0, "Pinning thread to proc %d of %d, local rank is %d\n",
			desiredCpu, numCores, localRank);

	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(desiredCpu, &cpuSet);
	sched_setaffinity(0, sizeof(cpuSet), &cpuSet);
}

struct threadArgs {
	int solverId;
	HordeLib* hlib;
};

void* solverRunningThread(void* arg) {
	threadArgs* targs = (threadArgs*)arg;
	HordeLib* hlib = targs->hlib;
	PortfolioSolverInterface* solver = hlib->solvers[targs->solverId];
	delete targs;
	if (hlib->params.isSet("pin")) {
		hlib->interruptLock.lock();
		pinThread(hlib->params.getIntParam("c", 1));
		hlib->interruptLock.unlock();
	}
	while (true) {
		hlib->interruptLock.lock();
		if (hlib->solvingDoneLocal) {
			hlib->interruptLock.unlock();
			break;
		} else {
			solver->unsetSolverInterrupt();
		}
		hlib->interruptLock.unlock();
		//log(0, "rank %d starting solver with %d new lits, %d assumptions: %d\n", hlib->mpi_rank, litsAdded, hlib->assumptions.size(), hlib->assumptions[0]);
		litsAdded = 0;
		hlib->finalResult = UNKNOWN;
		SatResult res = solver->solve(hlib->assumptions);
		if (res == SAT) {
			hlib->solvingDoneLocal = true;
			log(0,"Solved by solver ID %d\n", solver->solverId);
			hlib->finalResult = SAT;
			hlib->truthValues = solver->getSolution();
		}
		if (res == UNSAT) {
			hlib->solvingDoneLocal = true;
			log(0,"Solved by solver ID %d\n", solver->solverId);
			hlib->finalResult = UNSAT;
			hlib->failedAssumptions = solver->getFailedAssumptions();
		}
	}
	return NULL;
}

void HordeLib::sparseDiversification(int mpi_size, int mpi_rank) {
	int totalSolvers = mpi_size * solversCount;
    int vars = solvers[0]->getVariablesCount();
    for (int sid = 0; sid < solversCount; sid++) {
    	int shift = (mpi_rank * solversCount) + sid;
		for (int var = 1; var + totalSolvers < vars; var += totalSolvers) {
			solvers[sid]->setPhase(var + shift, true);
		}
    }
}

void HordeLib::randomDiversification(unsigned int seed) {
	srand(seed);
    int vars = solvers[0]->getVariablesCount();
    for (int sid = 0; sid < solversCount; sid++) {
		for (int var = 1; var <= vars; var++) {
			solvers[sid]->setPhase(var, rand()%2 == 1);
		}
    }
}

void HordeLib::sparseRandomDiversification(unsigned int seed, int mpi_size) {
	srand(seed);
	int totalSolvers = solversCount * mpi_size;
    int vars = solvers[0]->getVariablesCount();
    for (int sid = 0; sid < solversCount; sid++) {
		for (int var = 1; var <= vars; var++) {
			if (rand() % totalSolvers == 0) {
				solvers[sid]->setPhase(var, rand() % 2 == 1);
			}
		}
    }
}

void HordeLib::nativeDiversification(int mpi_rank, int mpi_size) {
    for (int sid = 0; sid < solversCount; sid++) {
    	solvers[sid]->diversify(solvers[sid]->solverId, mpi_size*solversCount);
    }
}

void HordeLib::binValueDiversification(int mpi_size, int mpi_rank) {
	int totalSolvers = mpi_size * solversCount;
	int tmp = totalSolvers;
	int log = 0;
	while (tmp) {
		tmp >>= 1;
		log++;
	}
    int vars = solvers[0]->getVariablesCount();
    for (int sid = 0; sid < solversCount; sid++) {
    	int num = mpi_rank * sid;
		for (int var = 1; var < vars; var++) {
			int bit = var % log;
			bool phase = (num >> bit) & 1 ? true : false;
			solvers[sid]->setPhase(var, phase);
		}
    }
}

int HordeLib::solve() {
	solvingDoneLocal = false;
	for (int i = 0; i < solversCount; i++) {
		threadArgs* arg = new threadArgs();
		arg->hlib = this;
		arg->solverId = i;
		solverThreads[i] = new Thread(solverRunningThread, arg);
	}

	double startSolving = getTime();
	log(1, "Node %d started its solvers, initialization took %.2f seconds.\n", mpi_rank, startSolving);

	int maxSeconds = params.getIntParam("t", 0);
	size_t maxRounds = params.getIntParam("r", 0);
	size_t round = 1;

	while (true) {
		double timeNow = getTime();
		if (sleepInt > 0) {
			usleep(sleepInt);
			//log(0, "Node %d entering round %d (%.2f seconds solving, %.2f rounds/sec)\n", mpi_rank, round,
					//timeNow - startSolving, round/(timeNow - startSolving));
		}
		if (endingFunction(mpi_size, mpi_rank, solvingDoneLocal)) {
			stopAllSolvers();
			break;
		}
		if (sharingManager != NULL) {
			sharingManager->doSharing();
		}
		if (round == maxRounds || (maxSeconds != 0 && timeNow > maxSeconds)) {
			endingFunction = getGlobalEnding;
			solvingDoneLocal = true;
		}
		fflush(stdout);
		round++;
	}
	double searchTime = getTime() - startSolving;
	log(0, "node %d finished, joining solver threads\n", mpi_rank);
	for (int i = 0; i < solversCount; i++) {
		solverThreads[i]->join();
	}
	MPI_Barrier(MPI_COMM_WORLD);

	if (params.isSet("stats")) {
		// Statistics gathering
		// Local statistics
		SolvingStatistics locSolveStats;
		for (int i = 0; i < solversCount; i++) {
			SolvingStatistics st = solvers[i]->getStatistics();
			log(1, "thread-stats node:%d/%d thread:%d/%d props:%lu decs:%lu confs:%lu mem:%0.2f\n",
					mpi_rank, mpi_size, i, solversCount, st.propagations, st.decisions, st.conflicts, st.memPeak);
			locSolveStats.conflicts += st.conflicts;
			locSolveStats.decisions += st.decisions;
			locSolveStats.memPeak += st.memPeak;
			locSolveStats.propagations += st.propagations;
			locSolveStats.restarts += st.restarts;
		}
		SharingStatistics locShareStats;
		if (sharingManager != NULL) {
			locShareStats = sharingManager->getStatistics();
		}
		log(1, "node-stats node:%d/%d solved:%d res:%d props:%lu decs:%lu confs:%lu mem:%0.2f shared:%lu filtered:%lu\n",
				mpi_rank, mpi_size, finalResult != 0, finalResult, locSolveStats.propagations, locSolveStats.decisions,
				locSolveStats.conflicts, locSolveStats.memPeak, locShareStats.sharedClauses, locShareStats.filteredClauses);
		if (mpi_size > 1) {
			// Global statistics
			SatResult globalResult;
			MPI_Reduce(&finalResult, &globalResult, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
			SolvingStatistics globSolveStats;
			MPI_Reduce(&locSolveStats.propagations, &globSolveStats.propagations, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locSolveStats.decisions, &globSolveStats.decisions, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locSolveStats.conflicts, &globSolveStats.conflicts, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locSolveStats.memPeak, &globSolveStats.memPeak, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
			SharingStatistics globShareStats;
			MPI_Reduce(&locShareStats.sharedClauses, &globShareStats.sharedClauses, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locShareStats.importedClauses, &globShareStats.importedClauses, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locShareStats.filteredClauses, &globShareStats.filteredClauses, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
			MPI_Reduce(&locShareStats.dropped, &globShareStats.dropped, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

			if (mpi_rank == 0) {
				log(0, "glob-stats nodes:%d threads:%d solved:%d res:%d rounds:%lu time:%.2f mem:%0.2f MB props:%.2f decs:%.2f confs:%.2f "
					"shared:%.2f imported:%.2f filtered:%.2f dropped:%.2f\n",
					mpi_size, solversCount, globalResult != 0, globalResult, round,
					searchTime, globSolveStats.memPeak,
					globSolveStats.propagations/searchTime, globSolveStats.decisions/searchTime, globSolveStats.conflicts/searchTime,
					globShareStats.sharedClauses/searchTime, globShareStats.importedClauses/searchTime, globShareStats.filteredClauses/searchTime, globShareStats.dropped/searchTime);
			}
		}
	}
	//log(0, "rank %d result:%d\n", mpi_rank, finalResult);
	assumptions.clear();
	return finalResult;
}

bool HordeLib::readFormula(const char* filename) {
	if (params.isSet("qbf")) {
		for (size_t i = 0; i < solvers.size(); i++) {
			solvers[i]->loadFormula(filename);
		}
	} else {
		loadFormulaToSolvers(solvers, filename);
	}
	return true;
}

// incremental iface
void HordeLib::addLit(int lit) {
	litsAdded++;
	for (int i = 0; i < solversCount; i++) {
		solvers[i]->addLiteral(lit);
	}
}

void HordeLib::assume(int lit) {
	assumptions.push_back(lit);
}

int HordeLib::value(int lit) {
	return truthValues[abs(lit)];
}

int HordeLib::failed(int lit) {
	return failedAssumptions.find(lit) != failedAssumptions.end();
}


HordeLib::HordeLib(int argc, char** argv) {
	solverThreads = NULL;
	endingFunction = NULL;
	sharingManager = NULL;
	params.init(argc, argv);

	MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

	setVerbosityLevel(params.getIntParam("v", 1));
	//if (mpi_rank == 0) setVerbosityLevel(3);

	char hostname[1024];
	gethostname(hostname, 1024);
	log(0, "Initializing HordeSat ($Revision: 159 $) on host %s rank %d/%d input %s with parameters: ",
			hostname, mpi_rank, mpi_size, params.getFilename());
	params.printParams();

	solversCount = params.getIntParam("c", 1);
	//printf("solvers is %d", solversCount);

	for (int i = 0; i < solversCount; i++) {
		if (params.isSet("qbf")) {
			solvers.push_back(new DepQBF());
			log(1, "Running DepQBF on core %d of node %d/%d\n", i, mpi_rank, mpi_size);
		} else if (params.getParam("s") == "minisat") {
			solvers.push_back(new MiniSat());
			log(1, "Running MiniSat on core %d of node %d/%d\n", i, mpi_rank, mpi_size);
		} else if (params.getParam("s") == "combo") {
			if ((mpi_rank + i) % 2 == 0) {
				solvers.push_back(new MiniSat());
				log(1, "Running MiniSat on core %d of node %d/%d\n", i, mpi_rank, mpi_size);
			} else {
				solvers.push_back(new Lingeling());
				log(1, "Running Lingeling on core %d of node %d/%d\n", i, mpi_rank, mpi_size);
			}
		} else {
			solvers.push_back(new Lingeling());
			log(1, "Running Lingeling on core %d of node %d/%d\n", i, mpi_rank, mpi_size);
		}
		// set solver id
		solvers[i]->solverId = i + solversCount * mpi_rank;
	}

	sleepInt = 1000 * params.getIntParam("i", 1000);
	endingFunction = getGlobalEnding;
	if (mpi_size == 1) {
		endingFunction = getSingleProcessEnding;
		sleepInt = 50*1000;
	}


	int exchangeMode = params.getIntParam("e", 1);
	sharingManager = NULL;
	if (exchangeMode == 0) {
		log(1, "Clause sharing disabled.\n");
		//endingFunction = getGlobalEndingAsynchronous;
		sleepInt = 50*1000;
	} else if (mpi_size > 1 || solversCount > 1) {
		switch (exchangeMode) {
		case 1:
			sharingManager = new AllToAllSharingManager(mpi_size, mpi_rank, solvers, params);
			log(1, "Initialized all-to-all clause sharing.\n");
			break;
		case 2:
			sharingManager = new LogSharingManager(mpi_size, mpi_rank, solvers, params);
			log(1, "Initialized log-partners clause sharing.\n");
			break;
		case 3:
			sharingManager = new AsyncRumorSharingManager(mpi_size, mpi_rank, solvers, params);
			if (!params.isSet("i")) {
				sleepInt = 50*1000;
			}
			endingFunction = getGlobalEndingAsynchronous;
			log(1, "Initialized Asynchronous clause sharing.\n");
			break;
		}
	}

	if (params.isSet("neverend")) {
		endingFunction = getNeverEnding;
	}

	int diversification = params.getIntParam("d", 1);
	if (params.isSet("qbf")) {
		diversification = 4;
	}

	switch (diversification) {
	case 1:
		sparseDiversification(mpi_size, mpi_rank);
		log(1, "doing sparse diversification\n");
		break;
	case 2:
		binValueDiversification(mpi_size, mpi_rank);
		log(1, "doing binary value based diversification\n");
		break;
	case 3:
		randomDiversification(2015);
		log(1, "doing random diversification\n");
		break;
	case 4:
		nativeDiversification(mpi_rank, mpi_size);
		log(1, "doing native diversification (plingeling)\n");
		break;
	case 5:
		sparseDiversification(mpi_size, mpi_rank);
		nativeDiversification(mpi_rank, mpi_size);
		log(1, "doing sparse + native diversification\n");
		break;
	case 6:
		sparseRandomDiversification(mpi_rank, mpi_size);
		log(1, "doing sparse random diversification\n");
		break;
	case 7:
		sparseRandomDiversification(mpi_rank, mpi_size);
		nativeDiversification(mpi_rank, mpi_size);
		log(1, "doing random sparse + native diversification (plingeling)\n");
		break;
	case 0:
		log(1, "no diversification\n");
		break;
	}

	solverThreads = (Thread**) malloc (solversCount*sizeof(Thread*));
}

HordeLib::~HordeLib() {
	// Cleanup
	for (int i = 0; i < solversCount; i++) {
		delete solverThreads[i];
	}
	free(solverThreads);
	delete sharingManager;
}

