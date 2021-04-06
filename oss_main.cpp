/********************************************
 * oss_main - Process Scheduling (oss) Application
 * This file is for the main function of the
 * application.  It simply makes sure the
 * program arguements are correct, then
 * kicks off the oss functionality for
 * processing.
 * 
 * Brett Huffman
 * CMP SCI 4760 - Project 4
 * Due Mar 29, 2021
 * Main CPP file for oss project
 ********************************************/
#include <iostream>
#include <string.h>
#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "productSemaphores.h"
#include "oss.h"

// Forward declarations
static void show_usage(std::string);

using namespace std;

// Main - expecting arguments
int main(int argc, char* argv[])
{
    // This main area will only handle the processing
    // of the incoming arguments.

//    string strLog =  "OSS app by Brett Huffman for CMP SCI 4760";
//    cout << endl << strLog << endl << endl;

    // Argument processing
    int opt;
    string strLogFile = "logfile";
    int nNumberOfSeconds = 300;

    // Go through each parameter entered and
    // prepare for processing
    while ((opt = getopt(argc, argv, "hs:l")) != -1) {
        switch (opt) {
            case 'h':
                show_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'l':
                strLogFile = optarg;
                break;
            case 's':
                nNumberOfSeconds = atoi(optarg);
                break;
            case '?': // Unknown arguement                
                if (isprint (optopt))
                {
                    errno = EINVAL;
                    perror("Unknown option");
                }
                else
                {
                    errno = EINVAL;
                    perror("Unknown option character");
                }
                return EXIT_FAILURE;
            default:    // An bad input parameter was entered
                // Show error because a bad option was found
                perror ("oss: Error: Illegal option found");
                show_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    return ossProcess(strLogFile, nNumberOfSeconds);
}


// Handle errors in input arguments by showing usage screen
static void show_usage(std::string name)
{
    std::cerr << std::endl
              << name << " - oss app by Brett Huffman for CMP SCI 4760" << std::endl
              << std::endl
              << "Usage:\t" << name << " [-h]" << std::endl
              << "\t" << name << " [-s t] [-l f]" << std::endl
              << "Options:" << std::endl
              << "  -h   Describe how the project should be run, then terminate" << std::endl
              << "  -s t Indicate how many maximum seconds before the system terminates" << std::endl
              << "  -l f Specify a particular name for the log file (Default logfile)" << std::endl
              << std::endl << std::endl;
}