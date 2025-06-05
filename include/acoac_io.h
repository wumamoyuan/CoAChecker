#ifndef _ACoACIO_H
#define _ACoACIO_H

#include "acoac_inst.h"

/**
 * Read an ACoAC instance from a file
 * 
 * @param filename[in]: The path of the file to read
 * @return The ACoAC instance
 */
ACoACInstance *readACoACInstance(char *filename);

/**
 * Read an ARBAC instance from a file and convert it to an ACoAC instance
 * 
 * @param filename[in]: The path of the file to read
 * @return The ACoAC instance
 */
ACoACInstance *readARBACInstance(char *filename);

/**
 * Write an ACoAC instance to a file
 * 
 * @param pInst[in]: The ACoAC instance to write
 * @param filename[in]: The path of the file to write
 * @return 0 if write successfully, error code if failed
 */
int writeACoACInstance(ACoACInstance *pInst, char *filename);

#endif // _ACoACIO_H