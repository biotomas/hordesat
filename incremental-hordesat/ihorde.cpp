/*
 * ihorde.cpp
 *
 *  Created on: Nov 20, 2017
 *      Author: balyo
 */

#include "utilities/DimspecUtils.h"
#include "utilities/Logger.h"

int main(int argc, char **argv) {
	log(0"Usage: ./dss <dimspec-file>");
	DimspecFormula f = readDimspecProblem(argv[1]);
	checkDimspecValidity(f);
	DimspecSolution sol; //TODO = trivialSolver(f);
	if (checkDimspecSolution(f, sol)) {
		log(0, "Solution verified\n");
	}
	return 0;
}



