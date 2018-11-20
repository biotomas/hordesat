// Copyright (c) 2018 Markus Iser, Karlsruhe Institute of Technology
/*
 * Candy.cpp
 *
 *  Created on: Oct 18, 2018
 *      Author: markus
 */

#ifndef CANDY_H_
#define CANDY_H_

#include "PortfolioSolverInterface.h"
#include "../utilities/Threading.h"
#include "candy/simp/SimpSolver.h" 

#define CLS_COUNT_INTERRUPT_LIMIT 300

// some forward declatarations for Minisat
namespace Candy {
	class Lit;
	template<class T, class _Size> class vec;
}


class CandyHorde : public PortfolioSolverInterface {

private:
	Candy::SimpSolver<Candy::VSIDS>* solver;

	std::vector< std::vector<int> > learnedClausesToAdd;
	std::vector< std::vector<int> > clausesToAdd;
	Mutex clauseAddingLock;
	int learnedLimit;
	double random_seed;
	friend void learnCallback(const std::vector<Candy::Lit,int>& cls, void* issuer);
	friend std::vector<Candy::Lit> convertLiterals(std::vector<int> int_lits);

public:
	int myId;
	LearnedClauseCallback* callback;

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
	SatResult solve(const std::vector<int>& assumptions);

	// Add a (list of) permanent clause(s) to the formula
	void addClause(std::vector<int>& clause);
	void addClauses(std::vector<std::vector<int> >& clauses);
	void addInitialClauses(std::vector<std::vector<int> >& clauses);

	// Add a (list of) learned clause(s) to the formula
	// The learned clauses might be added later or possibly never
	void addLearnedClause(std::vector<int>& clauses);
	void addLearnedClauses(std::vector<std::vector<int> >& clauses);

	// Set a function that should be called for each learned clause
	void setLearnedClauseCallback(LearnedClauseCallback* callback, int solverId);

	// Request the solver to produce more clauses
	void increaseClauseProduction();

	// Get solver statistics
	SolvingStatistics getStatistics();
	// Diversify
	void diversify(int rank, int size);

	// constructor
	CandyHorde(int rank, int size);
	// destructor
	virtual ~CandyHorde();

};


#endif /* CANDY_H_ */
