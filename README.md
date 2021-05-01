# Memory Management (oss) Application

Memory Management (oss) application by Brett Huffman for CMP SCI 4760 - Project 6 - V1.0

In this project, 

A log was kept of each day's activity.  It is found at the end of this README file.

A Git repository was maintained with a public remote found here: https://github.com/dicer2000/MemoryManagement.git

## Assumptions
There were some items I didn't understand about the project's operation.  Based on the feedback I did receive, I made these assumptions:

1. All times are processed and shown in Seconds:Nanoseconds
2. The pages are being kept as an array of structs (called PageTable).  This simulates 32 1k pages.  I'm using these structs instead of byte arrays per instruction by Jared Diehl in 4/28 and 4/29 meetings.


## Program Switches
The program can be invoked as:

```
oss [-h] 
oss [-v]
  -h Describe how the project should be run, then terminate.
  -v puts the logfile output into Verbose Mode
```

## Install
To install this program, clone it with git to the folder to which you want 
it saved.
```
git clone https://github.com/dicer2000/MemoryManagement.git
```
## Compile
To compile the master application, simply run the make command:
```
make
```
## Run
To run the program, use the oss command.  You can use any of the command line options listed in program switches area.

## Problems / Issues

The biggest problems experienced in this project was in just understanding what this project was trying to do.  It took me reading and re-reading the instructions many times.  Finally, it became clear, but it was a push.

It seems there are many ways to interpret this project.  This code is the result of my interpretation and study with Jared Diehl.  Please see the assumptions above for key decisions made in the project.

## Work Log

- 4/21/2021 - Setup initial project files and make file
- 4/22/2021 - Creating child and PCB
- 4/23/2021 - Created PageTable and initialized; Research
- 4/24/2021 - Testing; Adding multi-process support
- 4/25/2021 - Investigating freezes; Testing; Fix
- 4/26/2021 - Modified Bilstmapper class to show memory in table format
- 4/27/2021 - Debugging
- 4/28/2021 - Adding logic for memory interrupts
- 4/29/2021 - Debugging; Completing memory setup; Handling page faulting; Logging
- 4/30/2021 - Changing how addresses are stored; Debugging
- 5/1/2021  - Fixing bugs when run on Linux; Debugging;

*©2021 Brett W. Huffman*