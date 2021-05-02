/********************************************
 * user_proc - Memory Management (oss) Application
 * This is the child process called by the oos
 * application.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 6
 * Due May 4, 2021
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
    if(argc < 3)
    {
        show_usage("user_Proc.cpp");
        cout << "Args: " << argv[0] << endl;
        perror("user_proc: Incorrect argument found");
        exit(EXIT_FAILURE);
    }

    // Get the incoming Queue ID of the process
    const int nItemToProcess = atoi(argv[1]);

    // And the log file string
    string strLogFile = argv[2];

    // Get the incoming Queue ID of the process
    const int nMaxProcessScheduleTime = atoi(argv[3]);

    // Register SIGQUIT handling
    signal(SIGINT, sigQuitHandler);

    // Attach to the control Semaphore
    productSemaphores s(KEY_MUTEX, false);
    if(!s.isInitialized())
    {
        perror("user_proc: Could not successfully find Semaphore");
        exit(EXIT_FAILURE);
    }

    // Pid used throughout child
    const pid_t nPid = getpid();

    // Seed the randomizer with the PID
    srand(time(0) ^ nPid);

    time_t secondsStart = time(NULL);   // Start time

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
    struct OssHeader* ossHeader = (struct OssHeader*) (shm_addr);

    // Log a new process started
    s.Wait();
    LogItem("PROC ", ossHeader->simClockSeconds,
        ossHeader->simClockNanoseconds, "Started Successfully", 
        nPid, nItemToProcess, strLogFile);
    s.Signal();

    // Loop forever, the first if statement will handle controlled shutdown
    while(true)
    {

        // Set probabilities for this round
        bool willShutdown = getRandomProbability(0.001f);
        bool willRead = getRandomProbability(readwriteProbability);
        bool willReadOutsideLegalPageTable = getRandomProbability(0.001f);
        

//        cout << "=> " << (rand()%1000)/10.0f << " : " << .001 * 100.0f << endl;

        // Every round gets 1-500ms for processing time
        s.Wait();
        ossHeader->simClockNanoseconds += getRandomValue(1000, 500000);
        s.Signal();

        // Shut down
        if(sigQuitFlag || willShutdown)
        {


            s.Wait();
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds,
                "Process Shutting Down", 
                nPid, nItemToProcess, strLogFile);
            s.Signal();

            // Send the message Synchronously - we want it to shutdown
            // all the resources, then exit cleanly
            msg.type = OSS_MQ_TYPE;
            msg.action = PROCESS_SHUTDOWN;
            msg.procIndex = nItemToProcess;
            msg.procPid = nPid;
            int n = msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT);

            // Once I get the reply back, we can continue to shutdown
            msgrcv(msgid, (void *) &msg, sizeof(message), nPid, 0); 

            return EXIT_SUCCESS;
        }
        
        // Request memory
        // Get memory address to request
        int memAddress = rand() % 32768;
        // Setup for a bad address
        if(willReadOutsideLegalPageTable)
            memAddress += rand() % 32768;;
        msg.type = OSS_MQ_TYPE;
        msg.action = (willRead) ? FRAME_READ : FRAME_WRITE;
        msg.procIndex = nItemToProcess;
        msg.procPid = nPid;
        msg.memoryAddress = memAddress;
        // Send a memory request
        int n = msgsnd(msgid, (void *) &msg, sizeof(message), 0);// IPC_NOWAIT);
        // Once we get the reply back, we can continue
        msgrcv(msgid, (void *) &msg, sizeof(message), nPid, 0);
        // Check if OSS is telling it to shutdown
        if(msg.action==PROCESS_SHUTDOWN)
        {
            s.Wait();
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds,
                "Memory error - outside valid page table. Shutting down process", 
                nPid, nItemToProcess, strLogFile);
            s.Signal();

            return EXIT_FAILURE;
        }

        s.Wait();
        LogItem("PROC ", ossHeader->simClockSeconds,
            ossHeader->simClockNanoseconds,
            "Memory Received - Continuing", 
            nPid, nItemToProcess, strLogFile);
        s.Signal();

    }
}


// Handle errors in input arguments by showing usage screen
static void show_usage(std::string name)
{
    std::cerr << std::endl
              << name << " - user_proc app by Brett Huffman for CMP SCI 4760" << std::endl
              << std::endl
              << "Usage:\t" << name << " [-h]" << std::endl
              << "\t" << name << " QueueID LogFileName MaxProcessScheduleTime " << std::endl
              << "Options:" << std::endl
              << "  -h   Describe how the project should be run, then terminate" << std::endl
              << std::endl << std::endl;
}