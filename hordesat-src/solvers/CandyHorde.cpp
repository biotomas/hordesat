// Copyright (c) 2018 Markus Iser, Karlsruhe Institute of Technology
/*
 * Candy.cpp
 *
 *  Created on: Oct 18, 2018
 *      Author: markus
 */

#include "CandyHorde.h"
#include "../utilities/DebugUtils.h"

#include "candy/core/CNFProblem.h"

#include "candy/simp/SimpSolver.h"
#include "candy/core/CandySolverInterface.h" 

#include "candy/core/ClauseDatabase.h"
#include "candy/core/Trail.h"
#include "candy/core/Propagate.h"
#include "candy/core/ConflictAnalysis.h"
#include "candy/core/branching/BranchingDiversificationInterface.h"
#include "candy/core/branching/VSIDS.h"
#include "candy/core/branching/LRB.h"

using namespace Candy;

// Macros for candy literal representation conversion
#define CANDY_LIT(lit) lit > 0 ? mkLit(lit-1, false) : mkLit((-lit)-1, true)
#define INT_LIT(lit) sign(lit) ? -(var(lit)+1) : (var(lit)+1)

std::vector<Candy::Lit> convertLiterals(std::vector<int> int_lits) {
	std::vector<Lit> candy_clause;
	for (int int_lit : int_lits) {
		candy_clause.push_back(CANDY_LIT(int_lit));
	}
	return candy_clause;
}

CandyHorde::CandyHorde(int rank, int size) : random_seed(rank) { 
	clause_db = new Candy::ClauseDatabase();
	assignment = new Candy::Trail();
	propagate = new Candy::Propagate(*clause_db, *assignment);
	learning = new Candy::ConflictAnalysis(*clause_db, *assignment);
	if (random_seed % 2 == 0) {
		double var_decay = 0.8;
		double max_var_decay = 0.95;
		VSIDS* vsids = new Candy::VSIDS(*clause_db, *assignment, var_decay, max_var_decay);
		branching = vsids;
		solver = new SimpSolver<ClauseDatabase, Trail, Propagate, ConflictAnalysis, VSIDS>(*clause_db, *assignment, *propagate, *learning, *vsids);
	}
	else {
		double step_size = 0.4;
		LRB* lrb = new Candy::LRB(*clause_db, *assignment, step_size);
		branching = lrb;
		solver = new SimpSolver<ClauseDatabase, Trail, Propagate, ConflictAnalysis, LRB>(*clause_db, *assignment, *propagate, *learning, *lrb);
	}
	//solver->disablePreprocessing();
	learnedLimit = 0;
	myId = 0;
	callback = NULL;
}

CandyHorde::~CandyHorde() {
	delete solver;
}

bool CandyHorde::loadFormula(const char* filename) {
	CNFProblem problem{};
	problem.readDimacsFromFile(filename);
	solver->addClauses(problem);
	return true;
}

//Get the number of variables of the formula
int CandyHorde::getVariablesCount() {
	return solver->nVars();
}


// Get a variable suitable for search splitting
int CandyHorde::getSplittingVariable() {
	return var(branching->getLastDecision()) + 1;
}

// Set initial phase for a given variable
void CandyHorde::setPhase(const int var, const bool phase) {
	branching->setPolarity(var-1, phase);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void CandyHorde::setSolverInterrupt() {
	solver->setInterrupt(true);
}

void CandyHorde::unsetSolverInterrupt() {
	solver->setInterrupt(false);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult CandyHorde::solve(const vector<int>& assumptions) {
	clauseAddingLock.lock();

	for (size_t ind = 0; ind < clausesToAdd.size(); ind++) {
		solver->addClause(convertLiterals(clausesToAdd[ind]));
	}
	clausesToAdd.clear();

	for (size_t ind = 0; ind < learnedClausesToAdd.size(); ind++) {
		solver->addClause(convertLiterals(learnedClausesToAdd[ind]), true);
	}
	learnedClausesToAdd.clear();

	clauseAddingLock.unlock();

	if (solver->isInConflictingState()) {
		printf("unsat when adding cls\n");
		return UNSAT;
	}

	lbool res = solver->solve(convertLiterals(assumptions));
	if (res == l_True) {
		return SAT;
	}
	if (res == l_False) {
		return UNSAT;
	}
	return UNKNOWN;
}

void CandyHorde::addClause(vector<int>& clause) {
	clauseAddingLock.lock();
	clausesToAdd.push_back(clause);
	clauseAddingLock.unlock();
	setSolverInterrupt();
}

void CandyHorde::addLearnedClause(vector<int>& clause) {
	clauseAddingLock.lock();
	if (clause.size() == 1) {
		clausesToAdd.push_back(clause);
	} else {
		learnedClausesToAdd.push_back(clause);
	}
	clauseAddingLock.unlock();
	if (learnedClausesToAdd.size() > CLS_COUNT_INTERRUPT_LIMIT) {
		setSolverInterrupt();
	}
}

void CandyHorde::addClauses(vector<vector<int>>& clauses) {
	clauseAddingLock.lock();
	clausesToAdd.insert(clausesToAdd.end(), clauses.begin(), clauses.end());
	clauseAddingLock.unlock();
	setSolverInterrupt();
}

void CandyHorde::addInitialClauses(vector<vector<int>>& clauses) {
	CNFProblem problem {};
	
	for (size_t ind = 0; ind < clauses.size(); ind++) {
		problem.readClause(convertLiterals(clauses[ind]));
	}

	solver->addClauses(problem);

	if (solver->isInConflictingState()) {
		printf("unsat when adding initial cls\n");
	}
}

void CandyHorde::addLearnedClauses(vector<vector<int> >& clauses) {
	clauseAddingLock.lock();
	for (size_t i = 0; i < clauses.size(); i++) {
		if (clauses[i].size() == 1) {
			clausesToAdd.push_back(clauses[i]);
		} else {
			learnedClausesToAdd.push_back(clauses[i]);
		}
	}
	clauseAddingLock.unlock();
	if (learnedClausesToAdd.size() > CLS_COUNT_INTERRUPT_LIMIT || clausesToAdd.size() > 0) {
		setSolverInterrupt();
	}
}

void learnCallback(void* state, int* clause) {
	CandyHorde* mp = (CandyHorde*)state;

	std::vector<int> ncls;

	for (int i = 0; clause[i] != 0; i++) {
		ncls.push_back(clause[i]);
	}

	if (ncls.size() > 1) {
		int glue = std::min(3, (int)ncls.size());
		ncls.insert(ncls.begin(), glue);
	}

	mp->callback->processClause(ncls, mp->myId);
}

void CandyHorde::setLearnedClauseCallback(LearnedClauseCallback* callback, int solverId) {
	this->callback = callback;
	this->learnedLimit = 3;
	myId = solverId;
	solver->setLearntCallback((void*)this, this->learnedLimit, learnCallback);
}

void CandyHorde::increaseClauseProduction() {
	learnedLimit++;
	solver->setLearntCallback((void*)this, this->learnedLimit, learnCallback);
}

SolvingStatistics CandyHorde::getStatistics() {
	SolvingStatistics st;
	// st.conflicts = solver->conflicts;
	// st.propagations = solver->propagations;
	// st.restarts = solver->starts;
	// st.decisions = solver->decisions;
	// st.memPeak = memUsedPeak();
	return st;
}
