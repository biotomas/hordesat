// Copyright (c) 2015 Tomas Balyo, Karlsruhe Institute of Technology
// Copyright (c) 2021 Norbert Manthey
/*
 * MergeSat.h
 *
 *  Created on: Oct 9, 2014
 *      Author: balyo
 */

#ifndef MINISAT_H_
#define MINISAT_H_

#include "PortfolioSolverInterface.h"
#include "../utilities/Threading.h"
using namespace std;

#define CLS_COUNT_INTERRUPT_LIMIT 300

// allow to modify the name of the namespace, if required
#ifndef MERGESAT_NSPACE
#define MERGESAT_NSPACE Minisat
#endif

// some forward declatarations for MergeSat
namespace MERGESAT_NSPACE {
	class Solver;
	class Lit;
	template<class T> class vec;
}


class MergeSatBackend : public PortfolioSolverInterface {

private:
	MERGESAT_NSPACE::Solver *solver;
	vector< vector<int> > learnedClausesToAdd;
	vector< vector<int> > clausesToAdd;
	Mutex clauseAddingLock;
	int myId;
	LearnedClauseCallback* callback;
	int learnedLimit;
	friend void miniLearnCallback(const std::vector<int>& cls, int glueValue, void* issuer);
	friend void consumeSharedCls(void* issuer);

public:

	bool loadFormula(const char* filename);
	//Get the number of variables of the formula
	int getVariablesCount();
	// Get a variable suitable for search splitting
	int getSplittingVariable();
	// Set initial phase for a given variable
	void setPhase(const int var, const bool phase);
	// Interrupt the SAT solving, so it can be started again with new assumptions
	void setSolverInterrupt();
	void unsetSolverInterrupt();

	// Solve the formula with a given set of assumptions
	// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
	SatResult solve(const vector<int>& assumptions);

	// Add a (list of) permanent clause(s) to the formula
	void addClause(vector<int>& clause);
	void addClauses(vector<vector<int> >& clauses);
	void addInitialClauses(vector<vector<int> >& clauses);

	// Add a (list of) learned clause(s) to the formula
	// The learned clauses might be added later or possibly never
	void addLearnedClause(vector<int>& clauses);
	void addLearnedClauses(vector<vector<int> >& clauses);

	// Set a function that should be called for each learned clause
	void setLearnedClauseCallback(LearnedClauseCallback* callback, int solverId);

	// Request the solver to produce more clauses
	void increaseClauseProduction();

	// Get solver statistics
	SolvingStatistics getStatistics();
	// Diversify
	void diversify(int rank, int size);

	void addInternalClausesToSolver();

	// constructor
	MergeSatBackend();
	// destructor
	virtual ~MergeSatBackend();

};


#endif /* MINISAT_H_ */
