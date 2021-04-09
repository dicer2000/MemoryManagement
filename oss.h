 /********************************************
 * oss App - Process Scheduling (oss) Application
 * This is the main functionality of the oss
 * application.  It is called by the oss_main.cpp
 * file.
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
int ossProcess(std::string, bool);

int forkProcess(string, string, int);

#endif // OSS_H