/*
 * Logger.h
 *
 *  Created on: Mar 9, 2015
 *      Author: balyo
 */

#ifndef LOGGER_H_
#define LOGGER_H_

double getTime();
double getAbsoluteTimeLP();
void setVerbosityLevel(int level);
void log(int verbosityLevel, const char* fmt ...);
void exitError(const char* fmt ...);


#endif /* LOGGER_H_ */
