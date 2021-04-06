 /********************************************
 * App - Process Scheduling (oss) Application
 * 
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 4
 * Due Mar 29, 2021
 * oss .h file for oss project
 ********************************************/
#ifndef OSS_H
#define OSS_H

#include <string>

using namespace std;

// ossProcess - Process to start oss process.
int ossProcess(std::string, int);

int forkProcess(string, string, int);

#endif // OSS_H