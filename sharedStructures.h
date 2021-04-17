/********************************************
 * sharedStructures - This is a file that
 * contains all the structures shared between
 * the oss & user_proc processes.  It
 * contains library calls, the main structure
 * containing data, and shared messages.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 5
 * Due Apr 20, 2021
 * sharedStructures.h file for project
 ********************************************/
#ifndef SHAREDSTRUCTURES_H
#define SHAREDSTRUCTURES_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <sys/ipc.h> 
#include <sys/msg.h> 
#include <string.h>
#include <stdarg.h>  // For va_start, etc.
#include "productSemaphores.h"
#include <assert.h>

//***************************************************
// Important Program Constants
//***************************************************

// The size of our product queue
const int maxTimeToRunInSeconds = 3;

const int PROCESSES_MAX = 40;
const int RESOURCES_MAX = 18;
const char* ChildProcess = "./user_proc";

//***************************************************
// Enums
//***************************************************

enum RequestType { REQUEST_CREATE, REQUEST_DESTROY, REQUEST_SHUTDOWN, OK };

//***************************************************
// Structures
//***************************************************

struct OssHeader {
    int simClockSeconds;     // System Clock - Seconds
    int simClockNanoseconds; // System Clock - Nanoseconds
    
    int allocatedMatrix[PROCESSES_MAX * RESOURCES_MAX];
    int availabilityMatrix[RESOURCES_MAX];
    int requestMatrix[PROCESSES_MAX * RESOURCES_MAX];
};

struct ResourceDescriptors {
    std::vector<int> allocatedProcs;
    std::vector<int> waitingQueue;
    int  countTotalResources;
    int  countRequested;
    int  countAllocated;
    int  countReleased;
    int  countWaited;
};

struct UserProcesses {
    int pid;
};

const key_t KEY_SHMEM = 0x54320;  // Shared key
int shm_id; // Shared Mem ident
char* shm_addr;

//***************************************************
// Message Queue
//***************************************************
const key_t KEY_MESSAGE_QUEUE = 0x54324;

// Structure for message queue 
struct message {
    long type;
    int  action;
    int  procPid;
    int  procIndex;
    int  resIndex;
} msg;

const long OSS_MQ_TYPE = 1000;

//***************************************************
// Semaphores
//***************************************************
const key_t KEY_MUTEX = 0x54321;

/***************************************************
 * Helper Functions
 * *************************************************/

void Print1DArray(const int* nArray, const int nArraySize, const int nCols)
{
    // Print the entire array in 2D
    for(int i = 0; i < nArraySize/nCols; i++)
    {
        for(int j = 0; j < nCols; j++)
            std::cout << nArray[i * nCols + j] << " ";
        std::cout << std::endl;
    }
}
// Gets the value of an int in a 1D array based on columns and rows
int Get1DArrayValue(const int* nArray, const int nRow, const int nCol, const int nTotalCols)
{
//    cout << 
//    assert(sizeof(nArray) >= (nRow * nColMax + nCol)*sizeof(int));
    return nArray[nRow * nTotalCols + nCol];
}

// Sets the value of an int in a 1D array based on columns and rows
void Set1DArrayValue(int* nArray, const int nRow, const int nCol, const int nTotalCols, int newValue)
{
//    assert(sizeof(nArray) >= (nRow * nColMax + nCol)*sizeof(int));

    nArray[nRow * nTotalCols + nCol] = newValue;
}

// For time formatting used throughout both programs
std::string GetTimeFormatted(const char* prePendString)
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[10];
    
    // Get time
    time (&rawtime);
    timeinfo = localtime (&rawtime);

    // Format time for HH:MM:SS
    strftime (buffer,80,"%T",timeinfo);

    std::string strReturn = prePendString;
    strReturn.append(buffer);
    return strReturn;
}

// Returns a string from an int
std::string GetStringFromInt(const int nVal)
{
    int length = snprintf( NULL, 0, "%d", nVal);
    char* sDep = (char*)malloc( length + 1 );
    snprintf( sDep, length + 1, "%d", nVal);
    std::string strFinalVal = sDep;                    
    free(sDep);
    return strFinalVal;
}

// Returns a string from an float to 4 decimal places
std::string GetStringFromFloat(const float nVal)
{
    int length = snprintf( NULL, 0, "%0.4f", nVal);
    char* sDep = (char*)malloc( length + 1 );
    snprintf( sDep, length + 1, "%0.4f", nVal);
    std::string strFinalVal = sDep;                    
    free(sDep);
    return strFinalVal;
}

// Log file writing helper function
bool WriteLogFile(std::string& logString, std::string LogFile)
{
    // Open a file to write
    std::ofstream logFile (LogFile.c_str(), std::ofstream::out | std::ofstream::app);
    if (logFile.is_open())
    {
        // Get the current local time
//        logFile << GetTimeFormatted("").c_str();
        logFile << " " << logString.c_str() << std::endl;
        logFile.close();
        return true;
    }
    else
    {
        perror("Unable to write to log file");
        return false;
    }
}

// Set a Bitmap's Byte values
void setBitmapByte(unsigned char* bitmap, int addr, bool value)
{
    if(value)
    {
        // Set the bit at this point in the bitmap
        bitmap[addr/8] |= (1 << (7 - (addr%8)));
    }
    else
    {
        // Clear the bit
        bitmap[addr/8] &= ~(1 << (7 - (addr%8)));
    }
}

// Get a Bitmap's Btye values
bool getBitmapByte(unsigned char* bitmap, int addr)
{
    // returns true or false based on whether value
    // is set to 1 or 0 in bitmap
    return (bitmap[addr/8] & (1 << (7 - (addr%8))));
}

// Toggling a Bitmap's Byte values
void toggleByte(unsigned char* bitmap, int addr)
{
    // Toggle the bit at this point in the bitmap
    bitmap[addr/8] ^= (1 << (7 - (addr%8)));
}

// Returns a random number between two values
int getRandomValue(int MinVal, int MaxVal)
{
    int range = MaxVal-MinVal+1 ;
    return rand()%range + MinVal ;
}

// Returns a random true/false based on the probability passed
// Be sure to run srand first!
bool getRandomProbability(float ProbabilityOfTrue)
{
  return rand()%100 < (ProbabilityOfTrue * 100);
}

// Formats a string with variable number of params
std::string string_format(const std::string fmt, ...)
{
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}

// Writes a log file
void LogItem(std::string input, std::string LogFileName)
{

    std::cout << input.c_str() << std::endl;

    // Open a file to write
    std::ofstream logFile (LogFileName.c_str(), 
            std::ofstream::out | std::ofstream::app);
    if (logFile.is_open())
    {
        logFile << input.c_str() << std::endl;
        logFile.close();
    }
    else
    {
        perror("Unable to write to log file");
    }
}

// Writes a special kind of log that logs exactly the same thing
// to both a log in a specified file and to the screen
void LogItem(std::string strSystem, int timeSeconds, int timeNanoseconds, 
    std::string mainText, int PID, int Index, std::string LogFileName)
{

    std::cout << string_format("%s%.2d %.6d:%.10d\t%s PID %d",
            strSystem.c_str(), 
            Index,
            timeSeconds, 
            timeNanoseconds, 
            mainText.c_str(), PID) << std::endl;

    // Open a file to write
    std::ofstream logFile (LogFileName.c_str(), 
            std::ofstream::out | std::ofstream::app);
    if (logFile.is_open())
    {
        logFile << string_format("%s%.2d %.6d:%.10d\t%s PID %d",
            strSystem.c_str(), 
            Index,
            timeSeconds, 
            timeNanoseconds, 
            mainText.c_str(), PID) << std::endl;
        logFile.close();
    }
    else
    {
        perror("Unable to write to log file");
    }
}

#endif // SHAREDSTRUCTURES_H