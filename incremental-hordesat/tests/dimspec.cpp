/*
 * dimspec.cpp
 *
 *  Created on: Nov 14, 2017
 *      Author: balyo
 */
#include <stdio.h>
#include <ctype.h>
#include <vector>

#include "../utilities/Logger.h"
#include "../utilities/DebugUtils.h"
#include "../ipasir.h"

using namespace std;



struct CnfFormula {
	int variables;
	vector<vector<int> > clauses;
};

struct DimspecFormula {
	CnfFormula init, goal, universal, transition;
};

struct DimspecSolution {
	vector<vector<int> > values;
};

DimspecFormula readDimspecProblem(const char* filename) {
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		exitError("Failed to open input file (%s)\n", filename);
	}
	DimspecFormula fla;
	CnfFormula* cf = NULL;

	int vars, cls;
	char kar;
	int c = 0;
	bool neg = false;
	vector<int> clause;

	while (c != EOF) {
		c = fgetc(f);

		// problem definition line
		if (c == 'i' || c == 'g' || c == 'u' || c == 't') {
			char pline[512];
			int i = 0;
			while (c != '\n') {
				pline[i++] = c;
				c = fgetc(f);
			}
			pline[i] = 0;
			if (3 != sscanf(pline, "%c cnf %d %d", &kar, &vars, &cls)) {
				exitError("Failed to parse the problem definition line (%s)\n", pline);
			}
			switch(kar) {
			case 'i': cf = &fla.init;
			break;
			case 'u': cf = &fla.universal;
			break;
			case 'g': cf = &fla.goal;
			break;
			case 't': cf = &fla.transition;
			break;
			default:
				exitError("Invalid formula identifier (%s)\n", pline);
			}
			cf->variables = vars;
			continue;
		}
		// comment
		if (c == 'c') {
			// skip this line
			while(c != '\n') {
				c = fgetc(f);
			}
			continue;
		}
		// whitespace
		if (isspace(c)) {
			continue;
		}
		// negative
		if (c == '-') {
			neg = true;
			continue;
		}
		// number
		if (isdigit(c)) {
			int num = c - '0';
			c = fgetc(f);
			while (isdigit(c)) {
				num = num*10 + (c-'0');
				c = fgetc(f);
			}
			if (neg) {
				num *= -1;
			}
			neg = false;
			if (num == 0) {
				cf->clauses.push_back(clause);
				clause.clear();
			} else {
				clause.push_back(num);
			}
		}
	}
	fclose(f);
	return fla;
}

void addClauses(void* solver, const vector<vector<int> >& clauses, int offset, int actVariable = -1) {
	for (size_t i = 0; i < clauses.size(); i++) {
		for (size_t j = 0; j < clauses[i].size(); j++) {
			int lit = clauses[i][j];
			lit = lit > 0 ? lit + offset : lit - offset;
			ipasir_add(solver, lit);
		}
		if (actVariable > 0) {
			ipasir_add(solver, actVariable + offset);
		}
		ipasir_add(solver, 0);
	}
}

void assumeGoal(void* solver, const vector<int>& goals, int offset) {
	for (size_t i = 0; i < goals.size(); i++) {
		int lit = goals[i];
		lit = lit > 0 ? lit + offset : lit - offset;
		ipasir_assume(solver, lit);
	}
}

DimspecSolution trivialSolver(const DimspecFormula& fla) {
	vector<int> goalAssumptions;
	bool longgoals = false;
	for (size_t i = 0; i < fla.goal.clauses.size(); i++) {
		if (fla.goal.clauses[i].size() > 1) {
			longgoals = true;
			break;
		} else {
			goalAssumptions.push_back(fla.goal.clauses[i][0]);
		}
	}
	int flaVariables = fla.init.variables;
	int activationVar = flaVariables+1;
	if (longgoals) {
		goalAssumptions.clear();
		goalAssumptions.push_back(-activationVar);
		log(0, "Problem has non-unit goal clauses\n");
	}

	void* solver = ipasir_init();

	// test solution at level 0
	addClauses(solver, fla.init.clauses, 0);
	addClauses(solver, fla.universal.clauses, 0);
	if (longgoals) {
		addClauses(solver, fla.goal.clauses, 0, activationVar);
	}
	assumeGoal(solver, goalAssumptions, 0);

	int satResult = ipasir_solve(solver);
	int iteration = 0;
	while (satResult != 10) {
		addClauses(solver, fla.transition.clauses, iteration * flaVariables);
		iteration++;
		addClauses(solver, fla.universal.clauses, iteration * flaVariables);
		if (longgoals) {
			addClauses(solver, fla.goal.clauses, iteration * flaVariables, activationVar);
		}
		assumeGoal(solver, goalAssumptions, iteration * flaVariables);
		log(0, "Starting iteration %d of SAT solving\n", iteration);
		satResult = ipasir_solve(solver);
	}

	log(0, "Solution found at iteration nr. %d\n", iteration);

	DimspecSolution sol;
	vector<int> levelSol;
	for (int i = 0; i <= iteration; i++) {
		levelSol.clear();
		levelSol.push_back(0);
		for (int var = 1; var <= flaVariables; var++) {
			int offVar = var + (i*flaVariables);
			if (ipasir_val(solver, offVar) > 0) {
				levelSol.push_back(var);
			} else {
				levelSol.push_back(-var);
			}
		}
		sol.values.push_back(levelSol);
	}

	ipasir_release(solver);
	return sol;
}


bool checkClauses(const vector<vector<int> >& clauses, const vector<int>& values) {
	for (size_t i = 0; i < clauses.size(); i++) {
		bool sat = false;
		for (size_t j = 0; j < clauses[i].size(); j++) {
			int lit = clauses[i][j];
			int var = lit > 0 ? lit : -lit;
			if (values[var] == lit) {
				sat = true;
				break;
			}
		}
		if (!sat) {
			return false;
		}
	}
	return true;
}

bool checkTransitionClauses(const vector<vector<int> >& clauses, const vector<int>& values1, const vector<int>& values2) {
	int variables = values1.size() - 1;
	for (size_t i = 0; i < clauses.size(); i++) {
		bool sat = false;
		for (size_t j = 0; j < clauses[i].size(); j++) {
			int lit = clauses[i][j];
			int var = abs(lit);
			if (var <= variables && lit == values1[var]) {
				sat = true;
				break;
			}
			if (var > variables) {
				int val = values2[var - variables];
				val = val > 0 ? val+variables : val - variables;
				if (val == lit) {
					sat = true;
					break;
				}
			}
		}
		if (!sat) {
			return false;
		}
	}
	return true;
}

bool checkSolution(const DimspecFormula& f, const DimspecSolution& s) {
	if (!checkClauses(f.init.clauses, s.values[0])) {
		exitError("Wrong solution: Initial conditions unsat\n");
	}
	if (!checkClauses(f.goal.clauses, s.values[s.values.size() - 1])) {
		exitError("Wrong solution: Goal conditions unsat\n");
	}
	for (size_t step = 0; step < s.values.size(); step++) {
		if (!checkClauses(f.universal.clauses, s.values[step])) {
			exitError("Wrong solution, universal clauses do not hold in step %d\n", step);
		}
	}
	for (size_t step = 0; step+1 < s.values.size(); step++) {
		if (!checkTransitionClauses(f.transition.clauses, s.values[step], s.values[step+1])) {
			exitError("Wrong solution, transitional clauses do not hold between steps %d and %d\n", step, step+1);
		}
	}
	return true;
}

int main(int argc, char **argv) {
	ipasir_setup(argc, argv);
	puts("Usage: ./dss <dimspec-file>");
	DimspecFormula f = readDimspecProblem(argv[1]);
	if (f.init.variables != f.goal.variables || f.init.variables != f.universal.variables ||
			2*f.init.variables != f.transition.variables) {
		exitError("Variable numbers inconsistent in I,G,U,T = %d, %d, %d, %d\n",
				f.init.variables, f.goal.variables, f.universal.variables, f.transition.variables);
	}
	log(0, "Formula loaded, it has %d variables, number of clauses in I,G,U,T is %d, %d, %d, %d\n",
			f.init.variables, f.init.clauses.size(), f.goal.clauses.size(), f.universal.clauses.size(),
			f.transition.clauses.size());
	DimspecSolution sol = trivialSolver(f);
	if (checkSolution(f, sol)) {
		log(0, "Solution verified\n");
	}
	ipasir_finalize();
}
