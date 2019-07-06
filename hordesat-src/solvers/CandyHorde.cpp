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

#include "candy/frontend/CandyBuilder.h"

using namespace Candy;

// Macros for candy literal representation conversion
#define CANDY_LIT(lit) lit > 0 ? Lit(Var(lit-1), false) : Lit(Var((-lit)-1), true)
#define INT_LIT(lit) sign(lit) ? -(var(lit)+1) : (var(lit)+1)

Candy::Cl convertLiterals(std::vector<int> int_lits) {
	std::vector<Lit> candy_clause;
	for (int int_lit : int_lits) {
		candy_clause.push_back(CANDY_LIT(int_lit));
	}
	return candy_clause;
}

CandyHorde::CandyHorde(int rank, int size) : random_seed(rank), interrupted(false) { 
	solver = initCandyThread(random_seed % 16);
	solver->setTermCallback(this, &CandyHorde::interruptedCallback);
	branching = solver->getBranchingUnit();
	learnedLimit = 0;
	myId = 0;
	callback = NULL;
}

CandyHorde::~CandyHorde() {
	delete solver;
}

Candy::CandySolverInterface* CandyHorde::initCandyThread(unsigned int num) {
	ClauseDatabaseOptions::opt_recalculate_lbd = false;
	SolverOptions::opt_sort_watches = ((num % 2) == 0);
	SolverOptions::opt_preprocessing = (num == 0);
	SolverOptions::opt_inprocessing = num + SolverOptions::opt_inprocessing;
	VariableEliminationOptions::opt_use_elim = ((num % 3) == 0);
	VariableEliminationOptions::opt_use_asymm = (num == 6) || (num == 7) || (num == 14) || (num == 15);
	switch (num) {
		case 0 : case 1 : //vsids
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = false;
			break;
		case 2 : case 3 : //lrb
			SolverOptions::opt_use_lrb = true;
			RSILOptions::opt_rsil_enable = false;
			break;
		case 4 : case 5 : //rsil
			SolverOptions::opt_vsids_var_decay = 0.75;
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = true;
			break;
		case 6 : case 7 : //vsids
			SolverOptions::opt_vsids_var_decay = 0.7;
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = false;
			break;
		case 8 : case 9 : //vsids
			SolverOptions::opt_vsids_var_decay = 0.6;
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = false;
			break;
		case 10 : case 11 : //lrb
			SolverOptions::opt_lrb_step_size = 0.7;
			SolverOptions::opt_lrb_min_step_size = 0.02;
			SolverOptions::opt_use_lrb = true;
			RSILOptions::opt_rsil_enable = false;
			break;
		case 12 : case 13 : //rsil
			GateRecognitionOptions::opt_gr_semantic = true; 
			RandomSimulationOptions::opt_rs_nrounds = 1048576 * 2;
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = true;
			break;
		case 14 : case 15 : //vsids
			SolverOptions::opt_vsids_var_decay = 0.5;
			SolverOptions::opt_vsids_max_var_decay = 0.99;
			SolverOptions::opt_use_lrb = false;
			RSILOptions::opt_rsil_enable = false;
			break;
	}
	return createSolver(false, SolverOptions::opt_use_lrb, RSILOptions::opt_rsil_enable);
}

bool CandyHorde::loadFormula(const char* filename) {
	CNFProblem problem{};
	problem.readDimacsFromFile(filename);
	solver->init(problem);
	return true;
}

//Get the number of variables of the formula
int CandyHorde::getVariablesCount() {
	return solver->getStatistics().nVars();
}


// Get a variable suitable for search splitting
int CandyHorde::getSplittingVariable() {
	return branching->getLastDecision().var() + 1;
}

// Set initial phase for a given variable
void CandyHorde::setPhase(const int var, const bool phase) {
	branching->setPolarity(var-1, phase);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void CandyHorde::setSolverInterrupt() {
	interrupted = true;
}

void CandyHorde::unsetSolverInterrupt() {
	interrupted = false;
}

bool CandyHorde::isInterrupted() {
	return interrupted;
}

int CandyHorde::interruptedCallback(void* this_pointer) {
	CandyHorde* self = static_cast<CandyHorde*>(this_pointer);
	return self->isInterrupted();
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult CandyHorde::solve(const vector<int>& assumptions) {
	clauseAddingLock.lock();

	CNFProblem problem;	
	Cl converted;

	for (std::vector<int> clause : clausesToAdd) {
		converted = convertLiterals(clause);
		problem.readClause(converted);
	}
	clausesToAdd.clear();

	for (std::vector<int> clause : learnedClausesToAdd) {
		converted = convertLiterals(clause);
		problem.readClause(converted);
	}
	learnedClausesToAdd.clear();

	solver->init(problem);
	clauseAddingLock.unlock();

	converted = convertLiterals(assumptions);
	solver->setAssumptions(converted);

	lbool res = solver->solve();
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
	
	Cl converted;
	for (std::vector<int> clause : clauses) {
		converted = convertLiterals(clause);
		problem.readClause(converted);
	}
	clausesToAdd.clear();

	solver->init(problem);
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
