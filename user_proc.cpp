/********************************************
 * user_proc - Resource Management (oss) Application
 * This is the child process called by the oos
 * application.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 5
 * Due Apr 20, 2021
 * user_proc CPP file for oss project
 ********************************************/

#include <iostream>
#include <unistd.h>
#include "sharedStructures.h"
#include "productSemaphores.h"
#include <fstream>
#include <stdlib.h>
#include <time.h>

// Forward declarations
static void show_usage(std::string);

// SIGQUIT handling
volatile sig_atomic_t sigQuitFlag = 0;
void sigQuitHandler(int sig){ // can be called asynchronously
  sigQuitFlag = 1; // set flag
}

using namespace std;

// Main - expecting arguments
int main(int argc, char* argv[])
{
    // This main area will only handle the processing
    // of the incoming arguments.

    // Check incoming arguements
    if(argc < 2)
    {
        cout << "Args: " << argv[0] << endl;
        perror("user_proc: Incorrect argument found");
        exit(EXIT_FAILURE);
    }
    // Get the incoming Queue ID of the process
    const int nItemToProcess = atoi(argv[1]);

    // And the log file string
    string strLogFile = argv[2];

    // Register SIGQUIT handling
    signal(SIGINT, sigQuitHandler);

    // Pid used throughout child
    const pid_t nPid = getpid();

    // Seed the randomizer with the PID
    srand(time(0) ^ nPid);

    //cout << "********************" << childType << endl;

    // Open the connection with the Message Queue
    // msgget creates a message queue 
    // and returns identifier 
    int msgid = msgget(KEY_MESSAGE_QUEUE, IPC_CREAT | 0666); 
    if (msgid == -1) {
        perror("user_proc: Error creating Message Queue");
        exit(EXIT_FAILURE);
    }

    // Open the connection to shared memory
    // Allocate the shared memory
    // And get ready for read/write
    // Get a reference to the shared memory, if available
    int shm_id = shmget(KEY_SHMEM, 0, 0);
    if (shm_id == -1) {
        perror("user_proc: Could not successfully find Shared Memory");
        exit(EXIT_FAILURE);
    }

    // Read the memory size and calculate the array size
    struct shmid_ds shmid_ds;
    shmctl(shm_id, IPC_STAT, &shmid_ds);
    size_t realSize = shmid_ds.shm_segsz;

    // Now we have the size - actually setup with shmget
    shm_id = shmget(KEY_SHMEM, realSize, 0);
    if (shm_id == -1) {
        perror("user_proc: Could not successfully find Shared Memory");
        exit(EXIT_FAILURE);
    }

    // attach the shared memory segment to our process's address space
    shm_addr = (char*)shmat(shm_id, NULL, 0);
    if (!shm_addr) { /* operation failed. */
        perror("user_proc: Could not successfully attach Shared Memory");
        exit(EXIT_FAILURE);
    }

    // Get the queue header
    struct OssHeader* ossHeader = 
        (struct OssHeader*) (shm_addr);
    // Get our entire queue - HA! Got the struct to align right
    struct OssItem*ossItemQueue = 
        (struct OssItem*) (shm_addr+sizeof(OssHeader));

    // Log a new process started
    LogItem("PROC ", ossHeader->simClockSeconds,
        ossHeader->simClockNanoseconds, "Started Successfully", 
        nPid, nItemToProcess, strLogFile);

    // Loop until child process is stopped or it shuts down naturally
    while(!sigQuitFlag)
    {

        // Set probabilities for this round
        bool willInterrupt = false;
        bool willShutdown = getRandomProbability(0.15f);
        int nanoSecondsToShutdown = getRandomValue(200, 450);
        int nanoSecondsToInterrupt = getRandomValue(200, 450);   // Only used if interrupt happens
        if(ossItemQueue[nItemToProcess].PCB.processType == CPU)
            // CPU Bound process - less likely to get interrupted
            willInterrupt = getRandomProbability(0.10f) ? true : false;
        else
            // IO Bound - much more likely to get interrupted
            willInterrupt = getRandomProbability(0.40f) ? true : false;


        msgrcv(msgid, (void *) &msg, sizeof(struct message) - sizeof(long), nPid, 0); 

        // Send back to oss
        if(willShutdown)
        {
            strcpy(msg.text, "Shutdown");
            ossItemQueue[nItemToProcess].PCB.totalCPUTime += nanoSecondsToShutdown;
            ossItemQueue[nItemToProcess].PCB.timeUsedLastBurst = nanoSecondsToShutdown;
            // Send the message
            msg.type = OSS_MQ_TYPE;
            int n = msgsnd(msgid, (void *) &msg, sizeof(struct message) - sizeof(long), 0);
            
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds + nanoSecondsToShutdown,
                "Shutting down process", 
                nPid, nItemToProcess, strLogFile);

            return EXIT_SUCCESS;
        }
        else if(willInterrupt)
        {
            // An interrupt happened
            strcpy(msg.text, "Block");
            ossItemQueue[nItemToProcess].PCB.totalCPUTime += nanoSecondsToInterrupt;
            ossItemQueue[nItemToProcess].PCB.timeUsedLastBurst = nanoSecondsToInterrupt;
            // Set the block time to the current sim time + block time
            ossItemQueue[nItemToProcess].PCB.blockTimeSeconds = 
                ossHeader->simClockSeconds + getRandomValue(0, 5);
            ossItemQueue[nItemToProcess].PCB.blockTimeNanoseconds = 
                ossHeader->simClockNanoseconds + getRandomValue(0, 1000);
            ossItemQueue[nItemToProcess].PCB.blockTotalTime += 
                ossItemQueue[nItemToProcess].PCB.blockTimeSeconds;
            // Log what happened
            string strChildInfo2 = "Process blocked until ";
            strChildInfo2.append(GetStringFromInt(ossItemQueue[nItemToProcess].PCB.blockTimeSeconds));
            strChildInfo2.append("s:");
            strChildInfo2.append(GetStringFromInt(ossItemQueue[nItemToProcess].PCB.blockTimeNanoseconds));
            strChildInfo2.append("ms");
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds + nanoSecondsToInterrupt, strChildInfo2, 
                nPid, nItemToProcess, strLogFile);  
        }
        else
        {
            // PROC Ran for entire time Quantum
            strcpy(msg.text, "Full");
            // the full time quantum was used, so add 10M ns
            ossItemQueue[nItemToProcess].PCB.totalCPUTime += fullTransactionTimeInNS;
            ossItemQueue[nItemToProcess].PCB.timeUsedLastBurst = fullTransactionTimeInNS;

            // Log it
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds + fullTransactionTimeInNS, "Full quantum use", 
                nPid, nItemToProcess, strLogFile);

        }

        // Actually send the message back
        msg.type = OSS_MQ_TYPE;
        int n = msgsnd(msgid, (void *) &msg, sizeof(struct message) - sizeof(long), 0);
        
    }
}


// Handle errors in input arguments by showing usage screen
static void show_usage(std::string name)
{
    std::cerr << std::endl
              << name << " - user_proc app by Brett Huffman for CMP SCI 4760" << std::endl
              << std::endl
              << "Usage:\t" << name << " [-h]" << std::endl
              << "\t" << name << " [-s t] [-l f]" << std::endl
              << "Options:" << std::endl
              << "  -h   Describe how the project should be run, then terminate" << std::endl
              << "  -s t Indicate how many maximum seconds before the system terminates" << std::endl
              << "  -l f Specify a particular name for the log file (Default logfile)" << std::endl
              << std::endl << std::endl;
}