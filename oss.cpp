/********************************************
 * oss App - Memory Management (oss) Application
 * This is the main functionality of the oss
 * application.  It is called by the oss_main.cpp
 * file.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 6
 * Due May 4, 2021
 * oss CPP file for oss project
 ********************************************/

#include <iostream>
#include <string.h>
#include <queue>
#include <list>
#include <algorithm>
#include <unistd.h>
#include "productSemaphores.h"
#include "sharedStructures.h"
#include "bitmapper.h"
#include "oss.h"

using namespace std;


// SIGINT handling
volatile sig_atomic_t sigIntFlag = 0;
void sigintHandler(int sig){ // can be called asynchronously
  sigIntFlag = 1; // set flag
}

// Forward Declarations
int forkProcess(string, string, int);
string GenerateMemLayout(const int, const PCB&);

// ossProcess - Process to start oss process.
int ossProcess(string strLogFile, int nProcessesRequested)
{
    // Make sure there are always no more than 20 processes
    nProcessesRequested = min(nProcessesRequested, PROCESSES_MAX);

cout << "*****" << nProcessesRequested << endl;
sleep(2);
    // Important items
    struct OssHeader* ossHeader;

    int wstatus;
    long nNextTargetStartTime = 0;   // Next process' target start time

    // Queues for managing processes
    queue<MemQueueItems> IOQueue;

    // Pid used throughout child
    const pid_t nPid = getpid();

    // Seed the randomizer with the PID
    srand(time(0) ^ nPid);

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
    LogItem("OSS by Brett Huffman - CMP SCI 4760 - Project 6\n", strLogFile);
    LogItem("------------------------------------------------\n", strLogFile);
   

    // Bitmap object for keeping track of children
    bitmapper bm(PROCESSES_MAX);
    bitmapper memory(totalMemory);

    // Register SIGINT handling
    signal(SIGINT, sigintHandler);
    bool isKilled = false;
    bool isShutdown = false;

    // Statistics
    int nProcessCount = 0;   // 100 MAX
    int nTotalProcessCount = 0;
    int nTotalTime = 0;
    int nLastIOProcessTime = 0;
    int nNumberMemoryAccesses = 0;
    int nNumberPageFaults = 0;
    int nNumberSegFaults = 0;
    int MemoryAccessesTotalTimeNS = 0;
    int MemoryAccessesTotalTimeS = 0;

    // Create a Semaphore to coordinate control
    productSemaphores s(KEY_MUTEX, true, 1);
    if(!s.isInitialized())
    {
        perror("OSS: Could not successfully create Semaphore");
        exit(EXIT_FAILURE);
    }

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
    int memSize = sizeof(struct OssHeader);

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

    // Fill the product header
    ossHeader->simClockSeconds = 0;
    ossHeader->simClockNanoseconds = 0;

    // Setup all the arrays
    // Setup all Descriptors per instructions
    for(int i=0; i < PROCESSES_MAX && !isShutdown; i++)
    {
        ossHeader->pcb[i].pid = -1;
        ossHeader->pcb[i].currentFrame = 0;
        for(int j=0; j < pageCount; j++)
        {
            ossHeader->pcb[i].ptable[j].frame = -1;
            ossHeader->pcb[i].ptable[j].reference = 0;
            ossHeader->pcb[i].ptable[j].protection = rand() % 2;
            ossHeader->pcb[i].ptable[j].dirty = 0;
            ossHeader->pcb[i].ptable[j].valid = 0;
        }
    }

    // For debugging
//    Print1DArray(ossHeader->availabilityMatrix, RESOURCES_MAX, RESOURCES_MAX);
//    isShutdown=true;

    try {   // Error trap

    // Start of main loop that will do the following
    // - Handle oss shutdown
    // - Create new processes in between 500-1000 msec intervals
    // - Handle child shutdowns
    // - Handle memory requests

    while(!isShutdown)
    {
        // Every loop gets 100-10000ns for processing time
        s.Wait();
        ossHeader->simClockNanoseconds += getRandomValue(10, 10000);
        if(ossHeader->simClockNanoseconds > 1000000000)
        {
            ossHeader->simClockSeconds += floor(ossHeader->simClockNanoseconds/1000000000);
            ossHeader->simClockNanoseconds -= 1000000000;
        }
        s.Signal();

        if(MemoryAccessesTotalTimeNS > 1000000000)
        {
            MemoryAccessesTotalTimeS += floor(MemoryAccessesTotalTimeNS/1000000000);
            MemoryAccessesTotalTimeNS -= 1000000000;
        }

        // ********************************************
        // Create New Processes
        // ********************************************
        // Check bitmap for room to make new processes
        if(nProcessCount < PROCESSES_MAX && !isKilled)
        {
            // Check if there is room for new processes
            // in the bitmap structure
            int nIndex = 0;
            for(;nIndex < PROCESSES_MAX; nIndex++)
            {
                if(!bm.getBitmapBits(nIndex))
                {
                    // Found one.  Create new process
                    int newPID = forkProcess(ChildProcess, strLogFile, nIndex);

                    // Set bit in bitmap
                    bm.setBitmapBits(nIndex, true);

                    // Protected setup and Log it
                    s.Wait();

                    // Setup Shared Memory for processing
                    ossHeader->pcb[nIndex].pid = newPID;

                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Generating new process", 
                        newPID,
                        nIndex, strLogFile);

                    // Log Process Status
                    LogItem("Startup Process PCB Index " + GetStringFromInt(nIndex), strLogFile);
                    LogItem(bm.getBitView(), strLogFile);
                    
                    // Every new process gets 1-500ms for scheduling time
                    ossHeader->simClockNanoseconds += getRandomValue(1000, 500000);
                    s.Signal();

                    // Increment how many have been made
                    nProcessCount++;
                    nTotalProcessCount++;
                }
            }
        }

        // ********************************************
        // Handle Ctrl-C or End Of Simulation
        // ********************************************
        // Terminate the process if CTRL-C is typed
        // or if the max time-to-process has been exceeded
        // but only send out messages to kill once
        if((sigIntFlag || time(NULL) - secondsStart > 10 || nTotalProcessCount > 40) && isKilled==false)
        {
            isKilled = true;

            // Send signal for every child process to terminate
            for(int nIndex=0;nIndex<PROCESSES_MAX;nIndex++)
            {
                // Send signal to close if they are in-process
                s.Wait();

                // Clear the wait queue
                while(!IOQueue.empty())
                {
                    MemQueueItems mqi = IOQueue.front();
                    IOQueue.pop();
                    if(mqi.address > -1 && mqi.pcb > -1)
                    {
                        // Send memory response to waiting process
                        msg.action = OK;
                        msg.type = ossHeader->pcb[mqi.pcb].pid;
                        msg.memoryAddress = 0;
                        int n = msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT);
                    }
                }


                if(bm.getBitmapBits(nIndex))
                {
                    // Kill it and update our bitmap
                    kill(ossHeader->pcb[nIndex].pid, SIGQUIT);
                    bm.setBitmapBits(nIndex, false);

                    // Send back the message to continue shutdown
//                    msg.action = PROCESS_SHUTDOWN;
//                    msg.type = ossHeader->pcb[nIndex].pid;
//                    int n = msgsnd(msgid, (void *) &msg, sizeof(message), IPC_NOWAIT);
                }
                s.Signal();
            }
            
            // We have notified children to terminate immediately
            // then let program shutdown naturally -- that way
            // shared resources are deallocated correctly
            if(sigIntFlag)
            {
                errno = EINTR;
                perror("Killing processes due to ctrl-c signal");
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

            // Find the PID and remove it from the bitmap
            s.Wait();
            for(int nIndex=0;nIndex<PROCESSES_MAX;nIndex++)
            {
                if(ossHeader->pcb[nIndex].pid == waitPID)
                {
                    // Clear out the PCB and all Frames for this
                    // shutting down process
                    ossHeader->pcb[nIndex].pid = -1;
                    ossHeader->pcb[nIndex].currentFrame = 0;
                    for(int j=0; j < pageCount; j++)
                    {
                        ossHeader->pcb[nIndex].ptable[j].frame = -1;
                        ossHeader->pcb[nIndex].ptable[j].reference = 0;
                        ossHeader->pcb[nIndex].ptable[j].protection = rand() % 2;
                        ossHeader->pcb[nIndex].ptable[j].dirty = 0;
                        ossHeader->pcb[nIndex].ptable[j].valid = 0;
                    }

                    // Reset to start over
                    ossHeader->pcb[nIndex].pid = 0;
                    bm.setBitmapBits(nIndex, false);
                    nProcessCount--;

                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Process signaled shutdown", 
                        waitPID,
                        nIndex, strLogFile);
                    LogItem("Shutdown Process PCB Index " + GetStringFromInt(nIndex), strLogFile);
                    LogItem(bm.getBitView(), strLogFile);

                    break;
                }
            }
            s.Signal();

        } else if (WIFSIGNALED(wstatus) && waitPID > 0) {
            cout << waitPID << " killed by signal " << WTERMSIG(wstatus) << endl;
        } else if (WIFSTOPPED(wstatus) && waitPID > 0) {
            cout << waitPID << " stopped by signal " << WTERMSIG(wstatus) << endl;
        } else if (WIFCONTINUED(wstatus) && waitPID > 0) {
        }

        // ********************************************
        // Manage Child Requests
        // ********************************************
        while(msgrcv(msgid, (void *) &msg, sizeof(message), OSS_MQ_TYPE, IPC_NOWAIT) > 0)
        {
            int nProcessID = msg.procPid;
            /*
            s.Wait();
            LogItem("OSS  ", ossHeader->simClockSeconds,
                ossHeader->simClockNanoseconds, "Received Message from Process " + GetStringFromInt(msg.procIndex) + " : " + GetStringFromInt(msg.action), 
                msg.procPid, msg.procIndex, strLogFile);
            s.Signal();
            */
            if(msg.action==PROCESS_SHUTDOWN)
            {
                s.Wait();
                LogItem("OSS  ", ossHeader->simClockSeconds,
                    ossHeader->simClockNanoseconds, "Process Shutdown Message " + GetStringFromInt(msg.procIndex) + " : " + GetStringFromInt(msg.action), 
                    msg.procPid, msg.procIndex, strLogFile);
                s.Signal();

                // Send back the message to continue shutdown
                msg.action = OK;
                msg.type = nProcessID;
                int n = msgsnd(msgid, (void *) &msg, sizeof(message), 0); //IPC_NOWAIT);
            }
            else if(msg.action==FRAME_READ || msg.action==FRAME_WRITE)
            {
                nNumberMemoryAccesses++;

                // Check if the memory address is out of range.  If so, throw and
                // fault and shutdown that process
                if(msg.memoryAddress < 0 || msg.memoryAddress > 32767)
                {
                    s.Wait();
                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Memory request of range found: " + GetStringFromInt(msg.memoryAddress) + " shutting down process", 
                        msg.procPid, msg.procIndex, strLogFile);
                    s.Signal();

                    nNumberSegFaults++;
                    // Send back the message to shutdown process
                    msg.action = PROCESS_SHUTDOWN;
                    msg.type = nProcessID;
                    int n = msgsnd(msgid, (void *) &msg, sizeof(message), 0); //IPC_NOWAIT);
                }
                else
                {

                    // First, check if the frame is already in our page table
                    int foundFrame = -1;
                    for(int i = 0; i < pageCount; i++)
                    {
                        // If we've found this memory in the ptable and it's valid...
                        if(ossHeader->pcb->ptable[i].frame==msg.memoryAddress && ossHeader->pcb->ptable[i].valid)
                        {
                            foundFrame = i;
                            if(msg.action==FRAME_WRITE)
                                ossHeader->pcb->ptable[i].dirty = 1;
                            break;
                        }
                    }

                    // Found the frame, grant it to the requesting client
                    if(foundFrame > -1)
                    {
                        s.Wait();
                        // Add approx 14 ms for each read/write
                        ossHeader->simClockNanoseconds += 14000000;
                        MemoryAccessesTotalTimeNS += 14000000;
                        LogItem("OSS  ", ossHeader->simClockSeconds,
                            ossHeader->simClockNanoseconds, "Received Memory Request " + GetStringFromInt(msg.memoryAddress) + " Found",
                            msg.procPid, msg.procIndex, strLogFile);
                        s.Signal();

                        // Memory aquired, continue
                        msg.action = OK;
                        msg.type = nProcessID;
                        msg.memoryAddress = msg.memoryAddress;
                        int n = msgsnd(msgid, (void *) &msg, sizeof(message), 0); //IPC_NOWAIT);
                    }
                    else
                    {   // Not found. Interrupt and Queue for disk retrieval
                        if(msg.procIndex > 0 && bm.getBitmapBits(msg.procIndex)
                            && msg.memoryAddress > 0)
                        {
                            // Page fault!!
                            nNumberPageFaults++;

                            MemQueueItems mqi;
                            mqi.pcb = msg.procIndex;
                            mqi.address = msg.memoryAddress;
                            IOQueue.push(mqi);
                            s.Wait();
                            // Add approx 14 ms for each read/write
                            ossHeader->simClockNanoseconds += 14000000;
                            MemoryAccessesTotalTimeNS += 14000000;
                            LogItem("OSS  ", ossHeader->simClockSeconds,
                                ossHeader->simClockNanoseconds, "Received Memory Request " + GetStringFromInt(msg.memoryAddress) + " Not Found\n\t Page Fault - Queued for Retreival", 
                                msg.procPid, msg.procIndex, strLogFile);
                            s.Signal();                
                        }
                    }
                }
            }
        }

        // ********************************************
        // I/O Responses
        // ********************************************
        // If we've had 14ms since last response, process next queue item
        s.Wait();
        if(ossHeader->simClockNanoseconds-nLastIOProcessTime > 14000000)
        {
            if(!IOQueue.empty())
            {
                MemQueueItems mqi = IOQueue.front();
                IOQueue.pop();
                if(mqi.address > -1 && mqi.pcb > -1)
                {
                    while(ossHeader->pcb[mqi.pcb].ptable[ossHeader->pcb[mqi.pcb].currentFrame].reference > 0)
                    {
                        // Set the reference = 0, (2nd Chance Caching)
                        ossHeader->pcb[mqi.pcb].ptable[ossHeader->pcb[mqi.pcb].currentFrame].reference=0;
                        // Maintain the circular reference
                        ossHeader->pcb[mqi.pcb].currentFrame = ++ossHeader->pcb[mqi.pcb].currentFrame % pageCount;
                    }
                    int nFreeFrame = ossHeader->pcb[mqi.pcb].currentFrame;  // Designate the new frame
                    // Increment the counter
                    ossHeader->pcb[mqi.pcb].currentFrame = ++ossHeader->pcb[mqi.pcb].currentFrame % pageCount;

                    // If writing to a Dirty page, add extra time for the write to memory
                    // before we destroy the current values
                    if(ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].dirty)
                    {
                        ossHeader->simClockNanoseconds += 14000000;
                        MemoryAccessesTotalTimeNS += 14000000;
                    }

                    // Now, reading the new value in
                    ossHeader->simClockNanoseconds += 14000000;
                    MemoryAccessesTotalTimeNS += 14000000;

                    // Set the Page data
                    ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].frame = mqi.address;
                    ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].reference = 1;
                    ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].protection = rand() % 2;
                    ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].dirty = 0;
                    ossHeader->pcb[mqi.pcb].ptable[nFreeFrame].valid = 1;

                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Memory Granted: Frame " + GetStringFromInt(nFreeFrame), 
                        ossHeader->pcb[mqi.pcb].pid, mqi.pcb, strLogFile);

                    LogItem(GenerateMemLayout(mqi.pcb, ossHeader->pcb[mqi.pcb]), strLogFile);

                    // Send memory response to waiting process
                    msg.action = OK;
                    msg.type = ossHeader->pcb[mqi.pcb].pid;
                    msg.memoryAddress = mqi.address;
                    int n = msgsnd(msgid, (void *) &msg, sizeof(message), 0); //IPC_NOWAIT);
                }
                else
                {
                    //*************** Error observed finding correct frame for memory
                    LogItem("OSS  ", ossHeader->simClockSeconds,
                        ossHeader->simClockNanoseconds, "Error observed finding correct frame for memory", 
                        ossHeader->pcb[mqi.pcb].pid, mqi.pcb, strLogFile);
                    isShutdown = true;
                }
            }
        }
        s.Signal();
    } // End of main loop
    } catch( ... ) {
        cout << "An error occured.  Shutting down shared resources" << endl;
    }

    // Breakdown shared memory
    // Dedetach shared memory segment from process's address space

    s.Wait();
    // Get the stats from the shared memory before we break it down
    nTotalTime = ossHeader->simClockSeconds;

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


    // Calc & Report the statistics
    if(nTotalTime > 0)
    {
        LogItem("________________________________\n", strLogFile);
        LogItem("OSS Statistics", strLogFile);
        float fltStat = (float)nNumberMemoryAccesses / (float)nTotalTime;
        LogItem("Number of memory accesses per second:\t\t\t" + GetStringFromFloat(fltStat), strLogFile);

        fltStat = (float)nNumberPageFaults / (float)nNumberMemoryAccesses;
        LogItem("Number of page faults per memory access:\t\t" + GetStringFromFloat(fltStat), strLogFile);
        
        fltStat = (float)nNumberMemoryAccesses/(float)MemoryAccessesTotalTimeS;
        LogItem("Average memory access speed:\t\t\t\t" + GetStringFromFloat(fltStat) + " Kbps", strLogFile);
        
        fltStat = (float)nNumberSegFaults / (float)nNumberMemoryAccesses;
        LogItem("Number of seg faults per memory access:\t\t\t" + GetStringFromFloat(fltStat), strLogFile);
    }
    s.Signal();
    cout << endl;

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
              execl(strProcess.c_str(), strProcess.c_str(), strLogFile.c_str(), "50", (char*)0);
            else
            {
              // Convert int to a c_str to send to exec
              string strArrayItem = GetStringFromInt(nArrayItem);
              execl(strProcess.c_str(), strProcess.c_str(), strArrayItem.c_str(), strLogFile.c_str(), "50", (char*)0);
            }

            fflush(stdout); // Mostly for debugging -> tty wasn't flushing
            exit(EXIT_SUCCESS);    // Exit from forked process successfully
        }
        else
            return pid; // Returns the Parent PID
}

string GenerateMemLayout(const int index, const PCB& pcb)
{
    string strReturn = "PCB ";
    strReturn.append(GetStringFromInt(index));
    strReturn.append("\tOcc\tRef\tDirty\n");
    for(int i=0; i < pageCount; i++)
    {
        strReturn.append("Fr ");
        strReturn.append(GetStringFromInt(i));
        strReturn.append("\t");
        strReturn.append(GetStringFromInt(pcb.ptable[i].valid));
        strReturn.append("\t");
        strReturn.append(GetStringFromInt(pcb.ptable[i].reference));
        strReturn.append("\t");
        strReturn.append(GetStringFromInt(pcb.ptable[i].dirty));
        strReturn.append("\n");
    }
    return strReturn;
}