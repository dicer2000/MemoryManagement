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
#include "deadlock.h"

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

    // The list of resources the process owns
    vector<int> vecOwnedResourceList;

    cout << "&&& New: " << nItemToProcess << " : " << strLogFile << endl;

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
    // Get our entire queue
    struct UserProcesses* ossUserProcesses = (struct UserProcesses*) (shm_addr+sizeof(struct OssHeader));
    // Get our entire queue
    struct ResourceDescriptors* ossResourceDescriptors = (struct ResourceDescriptors*) (ossUserProcesses+(sizeof(struct UserProcesses)*PROCESSES_MAX));

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
        bool willRequestResource = getRandomProbability((float)(nMaxProcessScheduleTime/100.0f));
        bool willCloseResource = getRandomProbability((float)(nMaxProcessScheduleTime/100.0f));
        bool willShutdown = getRandomProbability(0.50f);
        
        // Every round gets 1-500ms for processing time
        s.Wait();
        ossHeader->simClockNanoseconds += getRandomValue(1000, 500000);
        s.Signal();

        // Shut down => signal to OSS to release resources and remove (Only after at least 1 second)
        if(sigQuitFlag || (time(NULL) - secondsStart > 1 && willShutdown))
        {


            s.Wait();
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds,
                "Shutting down process", 
                nPid, nItemToProcess, strLogFile);
            s.Signal();

            // Send the message Synchronously - we want it to shutdown
            // all the resources, then exit cleanly
            msg.type = OSS_MQ_TYPE;
            msg.action = REQUEST_SHUTDOWN;
            msg.procIndex = nItemToProcess;
            msg.procPid = nPid;
            int n = msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT);

            // Once I get the reply back, we can continue to shutdown
            msgrcv(msgid, (void *) &msg, sizeof(message), nPid, 0); 

            return EXIT_SUCCESS;
        }
        
        // Resource Request
        if(willRequestResource)
        {
            // Get the resource being requested
            int nResource = getRandomValue(0, RESOURCES_MAX-1);

            s.Wait();
            LogItem("PROC ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds, "Requesting Resource: " + GetStringFromInt(nResource), 
                nPid, nItemToProcess, strLogFile);
            s.Signal();

            // Request a new resource
            msg.type = OSS_MQ_TYPE;
            msg.action = REQUEST_CREATE;
            msg.procIndex = nItemToProcess;
            msg.procPid = nPid;
            msg.resIndex = nResource;

            msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT); //IPC_NOWAIT
            // Wait for a response to come back
            msgrcv(msgid, (void *) &msg, sizeof(message), nPid, 0);
            // At this point, I now own the process
            cout << "PROC &&& Pushing new item" << endl;
            if(msg.action == OK)
                vecOwnedResourceList.push_back(nResource);
            continue;
        }


        if(vecOwnedResourceList.size() > 0 && willCloseResource)
        {
            if(vecOwnedResourceList.size() > 0)
            {
                int nItemToRemove = getRandomValue(0, vecOwnedResourceList.size()-1);

                s.Wait();
                LogItem("PROC ", ossHeader->simClockSeconds,
                    ossHeader->simClockNanoseconds, "Releasing Resource: " + nItemToRemove, 
                    nPid, nItemToProcess, strLogFile);
                s.Signal();

                msg.type = OSS_MQ_TYPE;
                msg.action = REQUEST_DESTROY;
                msg.procIndex = nItemToProcess;
                msg.procPid = nPid;
                msg.resIndex = vecOwnedResourceList[nItemToRemove];
                int n = msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT);

                // Once I get the reply back, remove the resource
                msgrcv(msgid, (void *) &msg, sizeof(message), nPid, 0); 


                // Push the item to the owned resource vector
                if(msg.action == OK)
                {
//                    cout << "PR dest: " << vecOwnedResourceList.begin()+nItemToRemove << endl;
                    vecOwnedResourceList.erase(vecOwnedResourceList.begin()+nItemToRemove);
                }
            }
        }

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