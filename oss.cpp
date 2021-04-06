/********************************************
 * oss App - Process Scheduling (oss) Application
 * This is the main functionality of the oss
 * application.  It is called by the oss_main.cpp
 * file.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 4
 * Due Mar 29, 2021
 * oss CPP file for oss project
 ********************************************/

#include <iostream>
#include <string.h>
#include <queue>
#include <list>
#include <algorithm>
#include <unistd.h>
#include "sharedStructures.h"
#include "productSemaphores.h"
#include "bitmapper.h"
#include "oss.h"

using namespace std;

// SIGINT handling
volatile sig_atomic_t sigIntFlag = 0;
void sigintHandler(int sig){ // can be called asynchronously
  sigIntFlag = 1; // set flag
}

const int MAX_PROCESSES = 100;

// ossProcess - Process to start oss process.
int ossProcess(string strLogFile, int timeInSecondsToTerminate)
{
    // Important items
    struct OssHeader* ossHeader;
    struct OssItem* ossItemQueue;
    int wstatus;
    long nNextTargetStartTime = 0;   // Next process' target start time

    // Queues for managing processes
    queue<int> readyQueue;
    list<int> blockedList;

    // Start Time for time Analysis
    // Get the time in seconds for our process to make
    // sure we don't exceed the max amount of processing time
    time_t secondsStart = time(NULL);   // Start time
    struct tm * curtime = localtime( &secondsStart );   // Will use for filename uniqueness
    strLogFile.append("_").append(asctime(curtime));
    replace(strLogFile.begin(), strLogFile.end(), ' ', '_');
    replace(strLogFile.begin(), strLogFile.end(), '?', '_');

    // Print out the header
    LogItem("------------------------------------------------\n", strLogFile);
    LogItem("OSS by Brett Huffman - CMP SCI 4760 - Project 4\n", strLogFile);
    LogItem("------------------------------------------------\n", strLogFile);


    // Bitmap object for keeping track of children
    bitmapper bm(QUEUE_LENGTH);

    // Check Input and exit if a param is bad
    if(timeInSecondsToTerminate < 1)
    {
        errno = EINVAL;
        perror("OSS: Unknown option");
        return EXIT_FAILURE;
    }

    // Register SIGINT handling
    signal(SIGINT, sigintHandler);
    bool isKilled = false;
    bool isShutdown = false;

    // Statistics
    int nProcessCount = 0;   // 100 MAX
    int nCPUProcessCount = 0;
    int nIOProcessCount = 0;
    uint nCPUTimeInSystem = 0;
    uint nIOTimeInSystem = 0;
    uint nCPU_CPUUtil = 0;
    uint nIO_CPUUtil = 0;
    uint nCPU_TimeWaitedBlocked = 0;
    uint nIO_TimeWaitedBlocked = 0;
    uint nTotalTime = 0;
    uint nCPU_IdleTime = 0;
    uint nTotalWaitTime = 0;

    // Create a Semaphore to coordinate control
//    productSemaphores s(KEY_MUTEX, true, 1);

    // Setup Message Queue Functionality
    // Note: The oss app will always have a type of 1
    int msgid = msgget(KEY_MESSAGE_QUEUE, IPC_CREAT | 0666); 
    if (msgid == -1) {
        perror("OSS: Error creating Message Queue");
        exit(EXIT_FAILURE);
    }

    // Setup shared memory
    // allocate a shared memory segment with size of 
    // Product Header + entire Product array
    int memSize = sizeof(OssHeader) + sizeof(OssItem) * QUEUE_LENGTH;
    shm_id = shmget(KEY_SHMEM, memSize, IPC_CREAT | IPC_EXCL | 0660);
    if (shm_id == -1) {
        perror("OSS: Error allocating shared memory");
        exit(EXIT_FAILURE);
    }

    // attach the shared memory segment to our process's address space
    shm_addr = (char*)shmat(shm_id, NULL, 0);
    if (!shm_addr) { /* operation failed. */
        perror("OSS: Error attaching shared memory");
        exit(EXIT_FAILURE);
    }
    // Get the queue header
    ossHeader = (struct OssHeader*) (shm_addr);
    // Get our entire queue
    ossItemQueue = (struct OssItem*) (shm_addr+sizeof(OssHeader));
    // Index to Item Currently Processing - Start at nothing processing
    int nIndexToCurrentChildProcessing = -1;

    // Fill the product header
    ossHeader->simClockSeconds = 0;
    ossHeader->simClockNanoseconds = 0;

    // Set all items in queue to empty
    for(int i=0; i < QUEUE_LENGTH; i++)
    {
        // Capture the statistics

        // Set as ready to process
        ossItemQueue[i].PCB.totalCPUTime = 0;
        ossItemQueue[i].pidAssigned = 0;
    }


    // Start of main loop that will do the following
    // - Handle oss shutdown
    // - Create new processes on avg of 1 sec intervals
    // - Handle child shutdowns
    // - Dispatch processes to run on round-robin basis
    // - Gather statistics of each process
    // - assorted other misc items
    while(!isShutdown)
    {
        // Every loop gets 100-10000ns for scheduling time
        ossHeader->simClockNanoseconds += getRandomValue(10, 10000);

        // ********************************************
        // Create New Processes
        // ********************************************
        // Check bitmap for room to make new processes
        if(nProcessCount < MAX_PROCESSES && !isKilled &&
            time(NULL) - secondsStart < 3)
        {
            // Check if there is room for new processes
            // in the bitmap structure
            int nIndex = 0;
            for(;nIndex < QUEUE_LENGTH; nIndex++)
            {
                if(!bm.getBitmapBits(nIndex))
                {
                    cout << endl << "####### New Process #######" << endl;

                    // Found one.  Create new process
                    int newPID = forkProcess(ChildProcess, strLogFile, nIndex);

                    // Setup Shared Memory for processing
                    ossItemQueue[nIndex].pidAssigned = newPID;
                    ossItemQueue[nIndex].bReadyToProcess = true;
                    ossItemQueue[nIndex].PCB.totalCPUTime = 0;
                    ossItemQueue[nIndex].PCB.totalSystemTime = 0;
                    ossItemQueue[nIndex].PCB.timeUsedLastBurst = 0;
                    ossItemQueue[nIndex].PCB.blockTotalTime = 0;
                    ossItemQueue[nIndex].PCB.waitStartTime = 
                        ossHeader->simClockNanoseconds; // For Wait Time Calc

                    // Decide if it's a CPU or IO bound process
                    // This probability (<.50 will generate more CPU)
                    if(getRandomProbability(percentageCPU))
                    {
                        ossItemQueue[nIndex].PCB.processType = IO;
                        nIOProcessCount++;
                    }
                    else
                    {
                        ossItemQueue[nIndex].PCB.processType = CPU;
                        nCPUProcessCount++;
                    }

                    // Set bit in bitmap
                    bm.setBitmapBits(nIndex, true);

                    // Push immediately onto the Ready Queue
                    readyQueue.push(nIndex);

                    // Increment how many have been made
                    nProcessCount++;



                    // Increment out next target to make a new process
                    nNextTargetStartTime+=1000000000;
                    cout << "Next Target Time: " << nNextTargetStartTime << endl;

                    // Log it
                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Generating new process", 
                        newPID,
                        nIndex, strLogFile);

                    // Log Process Status
                    LogItem(bm.getBitView(), strLogFile);

                    // Simulate time to add new process
                    ossHeader->simClockNanoseconds += 500000;

                    break;
                }
            }
        }

        // ********************************************
        // Handle Ctrl-C or End Of Simulation
        // ********************************************
        // Terminate the process if CTRL-C is typed
        // or if the max time-to-process has been exceeded
        // but only send out messages to kill once
        if((sigIntFlag || (time(NULL)-secondsStart) > timeInSecondsToTerminate) && !isKilled)
        {
            isKilled = true;
            // Send signal for every child process to terminate
            for(int nIndex=0;nIndex<QUEUE_LENGTH;nIndex++)
            {
                // Send signal to close if they are in-process
                if(bm.getBitmapBits(nIndex))
                {
                    // Kill it and update our bitmap
                    kill(ossItemQueue[nIndex].pidAssigned, SIGQUIT);
                    bm.setBitmapBits(nIndex, false);
                }
            }

            // We have notified children to terminate immediately
            // then let program shutdown naturally -- that way
            // shared resources are deallocated correctly
            if(sigIntFlag)
            {
                errno = EINTR;
                perror("Killing processes due to ctrl-c signal");
            }
            else
            {
                errno = ETIMEDOUT;
                perror("Killing processes due to timeout");
            }
        }


        // ********************************************
        // Handle Child Shutdowns
        // ********************************************
        // Check for a PID
        // Note :: We use the WNOHANG to call waitpid without blocking
        // If it returns 0, it does not have a PID waiting
        int waitPID = waitpid(-1, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);

        // No PIDs are in-process
        if (waitPID == -1)
        {
            isShutdown = true;
            continue;
        }

        // A PID Exited
        if (WIFEXITED(wstatus) && waitPID > 0)
        {
//            cout << "O: ********************* Exited: " << waitPID << endl;

            // Find the PID and remove it from the bitmap
            for(int nIndex=0;nIndex<QUEUE_LENGTH;nIndex++)
            {
                if(ossItemQueue[nIndex].pidAssigned == waitPID)
                {
                    // Update the overall statistics
                    if(ossItemQueue[nIndex].PCB.processType==CPU)
                    {
                        nCPU_CPUUtil=ossItemQueue[nIndex].PCB.totalCPUTime/100000;
                        nIO_TimeWaitedBlocked+=ossItemQueue[nIndex].PCB.blockTotalTime;
                    }
                    else
                    {
                        nIO_CPUUtil=ossItemQueue[nIndex].PCB.totalCPUTime/100000;
                        nCPU_TimeWaitedBlocked=ossItemQueue[nIndex].PCB.blockTotalTime;
                    }

                    // Reset to start over
                    ossItemQueue[nIndex].PCB.totalCPUTime = 0;
                    ossItemQueue[nIndex].pidAssigned = 0;
                    bm.setBitmapBits(nIndex, false);

                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Process signaled shutdown", 
                        waitPID,
                        nIndex, strLogFile);

                    // Simulate time to process item shutdown
                    ossHeader->simClockNanoseconds += 500000;

                    break;
                }
            }

        } else if (WIFSIGNALED(wstatus) && waitPID > 0) {
            cout << waitPID << " killed by signal " << WTERMSIG(wstatus) << endl;
        } else if (WIFSTOPPED(wstatus) && waitPID > 0) {
            cout << waitPID << " stopped by signal " << WTERMSIG(wstatus) << endl;
        } else if (WIFCONTINUED(wstatus) && waitPID > 0) {
        }

        // ********************************************
        // Manage BlockedList - Push aged BlockedList items onto the ReadyQueue
        // ********************************************
        if(!isKilled)
        {
            for(list<int>::iterator blItem = blockedList.begin(); 
                blItem != blockedList.end(); ++blItem)
            {
                int nItemToCheck = *blItem;

                // Is this item ready to unblock?
                if(ossHeader->simClockSeconds > ossItemQueue[nItemToCheck].PCB.blockTimeSeconds ||
                    (ossItemQueue[nItemToCheck].PCB.blockTimeSeconds==ossHeader->simClockSeconds
                    && ossHeader->simClockNanoseconds > ossItemQueue[nItemToCheck].PCB.blockTimeNanoseconds))
                {
                    // Reset the block time
                    ossItemQueue[nItemToCheck].PCB.blockTimeSeconds = 0;
                    ossItemQueue[nItemToCheck].PCB.blockTimeNanoseconds = 0;
                    blockedList.erase(blItem);

                    ossItemQueue[nItemToCheck].PCB.waitStartTime = 
                        ossHeader->simClockNanoseconds; // For Wait Time C

                    // Just put one
                    readyQueue.push(nItemToCheck);

                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Unblocked item", 
                        ossItemQueue[nItemToCheck].pidAssigned,
                        nItemToCheck, strLogFile);
                
                    // Increment System Clock for Un-blocking
                    ossHeader->simClockNanoseconds += 10000;
                    break;
                }
            }
        }

        // ********************************************
        // Dispatch processes to run on round-robin basis
        // Gather Stats
        // ********************************************
        // For figuring out Idle Time
        if(readyQueue.empty())
        {
            nCPU_IdleTime += 1;
        }
        // Now, really process the Dispatches
        else if(!isKilled)
        {
            // Get next item from the ready queue
            int nIndexToNextChildProcessing = readyQueue.front();  
            readyQueue.pop();
            // Check for bit set
            if(bm.getBitmapBits(nIndexToNextChildProcessing))
            {
                cout << endl << "####### Dispatching #######" << endl;
                LogItem("OSS  ", ossHeader->simClockSeconds,
                    ossHeader->simClockNanoseconds, "Dispatching process", 
                    ossItemQueue[nIndexToNextChildProcessing].pidAssigned,
                    nIndexToNextChildProcessing, strLogFile);

                nTotalWaitTime = ossHeader->simClockNanoseconds -
                    ossItemQueue[nIndexToNextChildProcessing].PCB.waitStartTime;

                // Dispatch it
                strcpy(msg.text, "Dispatch");
                msg.type = ossItemQueue[nIndexToNextChildProcessing].pidAssigned;
                int n = msgsnd(msgid, (void *) &msg, sizeof(struct message) - sizeof(long), 0);
                //------------------------------------
                // Message sent, waiting for response
                //------------------------------------
                msgrcv(msgid, (void *) &msg, sizeof(struct message) - sizeof(long), OSS_MQ_TYPE, 0); 
//                cout << "OSS: from child: " << msg.text << endl;

                // Update the statistics on what happened while PROC was running
                ossHeader->simClockNanoseconds += 
                    ossItemQueue[nIndexToNextChildProcessing].PCB.timeUsedLastBurst;

                // Child shutting down, so handle
                if(strcmp(msg.text, "Shutdown")==0)
                {
                    // Turns out we don't do much.  This is handled by the PID closing
                }
                else if(strcmp(msg.text, "Block")==0)
                {
                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Pushed item to blocked list", 
                        ossItemQueue[nIndexToNextChildProcessing].pidAssigned,
                        nIndexToNextChildProcessing, strLogFile);
                    // Put on Block List
                    blockedList.push_back(nIndexToNextChildProcessing);

                }
                else
                {
                    ossItemQueue[nIndexToNextChildProcessing].PCB.waitStartTime = 
                        ossHeader->simClockNanoseconds; // For Wait Time C
                    // Full Quantum Run, just requeue and keep going
                    readyQueue.push(nIndexToNextChildProcessing);
                }
            }
        }
            // Per the directions advance the timer by 1.xx seconds
            ossHeader->simClockSeconds++;
            ossHeader->simClockNanoseconds += getRandomValue(0, 1000);

            // Manage the Timers
            if(ossHeader->simClockNanoseconds > 1000000000) // A second has passed
            {
                ossHeader->simClockNanoseconds = ossHeader->simClockNanoseconds - 1000000000;
                ossHeader->simClockSeconds++;
            }        
    } // End of main loop

    // Get the stats from the shared memory before we break it down
    nTotalTime = ossHeader->simClockSeconds;

    // Breakdown shared memory
    // Dedetach shared memory segment from process's address space

    LogItem("________________________________\n", strLogFile);
    LogItem("OSS: De-allocating shared memory", strLogFile);

    if (shmdt(shm_addr) == -1) {
        perror("OSS: Error detaching shared memory");
    }

    // De-allocate the shared memory segment.
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("OSS: Error deallocating shared memory ");
    }
    LogItem("OSS: Shared memory De-allocated", strLogFile);

    // Destroy the Message Queue
    msgctl(msgid,IPC_RMID,NULL);

    LogItem("OSS: Message Queue De-allocated", strLogFile);

    if(nCPUProcessCount > 0 && nIOProcessCount > 0)
    {
        // Calc & Report the statistics
        LogItem("________________________________\n", strLogFile);
        LogItem("OSS Statistics", strLogFile);
        LogItem("\t\t\tI/O\t\tCPU", strLogFile);
        LogItem("Total\t\t\t" + GetStringFromInt(nIOProcessCount) + "\t\t" + GetStringFromInt(nCPUProcessCount), strLogFile);
        string strIOStat = GetStringFromFloat((float)nTotalTime/(float)nIOProcessCount);
        string strCPUStat = GetStringFromFloat((float)nTotalTime/(float)nCPUProcessCount);
        LogItem("Avg Sys Time\t\t" + strIOStat + "\t\t" + strCPUStat, strLogFile);
        strIOStat = GetStringFromFloat((float)nIO_CPUUtil/(float)nIOProcessCount);
        strCPUStat = GetStringFromFloat((float)nCPU_CPUUtil/(float)nCPUProcessCount);
        LogItem("Avg CPU Util\t\t" + strIOStat + "\t\t" + strCPUStat, strLogFile);
        strIOStat = GetStringFromFloat((float)nIO_TimeWaitedBlocked/(float)nIOProcessCount);
        strCPUStat = GetStringFromFloat((float)nCPU_TimeWaitedBlocked/(float)nCPUProcessCount);
        LogItem("Avg Time Blocked\t" + strIOStat + "\t\t" + strCPUStat, strLogFile);

        strCPUStat = GetStringFromFloat((float)nTotalWaitTime/(float)(nIOProcessCount+nCPUProcessCount));
        LogItem("Avg Wait Time:\t" + strIOStat + "ns", strLogFile);
        strCPUStat = GetStringFromFloat((float)nCPU_IdleTime);
        LogItem("CPU Idle Time:\t" + strCPUStat + "\n", strLogFile);
    }
    // Success!
    return EXIT_SUCCESS;
}

// ForkProcess - fork a process and return the PID
int forkProcess(string strProcess, string strLogFile, int nArrayItem)
{
        pid_t pid = fork();
        // No child made - exit with failure
        if(pid < 0)
        {
            // Signal to any child process to exit

            perror("OSS: Could not fork process");
            return EXIT_FAILURE;
        }
        // Child process here - Assign out it's work
        if(pid == 0)
        {
            // Execute child process without array arguements
            if(nArrayItem < 0)
              execl(strProcess.c_str(), strProcess.c_str(), strLogFile.c_str(), (char*)0);
            else
            {
              // Convert int to a c_str to send to exec
              string strArrayItem = GetStringFromInt(nArrayItem);
              execl(strProcess.c_str(), strProcess.c_str(), strArrayItem.c_str(), strLogFile.c_str(), (char*)0);
            }

            fflush(stdout); // Mostly for debugging -> tty wasn't flushing
            exit(EXIT_SUCCESS);    // Exit from forked process successfully
        }
        else
            return pid; // Returns the Parent PID
}

