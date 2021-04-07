/********************************************
 * productSemaphores - Semaphores and Message Passing
 * This is a special class to create & use
 * product semaphores with the app.
 * (c)2021 Brett Huffman
 * 
 * Brett Huffman
 * productSemaphores .h file for project
 ********************************************/
#ifndef PRODUCTSEMAPHORES
#define PRODUCTSEMAPHORES

#include <sys/sem.h>


class productSemaphores
{
    private:
    
        bool _bCreator;
        int _semid;
        bool _isInitialized;
        struct sembuf structSemaBuf;

    public:

    productSemaphores(key_t, bool, int = 1);
    ~productSemaphores();

    // Check if properly setup
    bool isInitialized() { return _isInitialized; };

    // Semaphore Wait
    void Wait();

    // Semaphore Signal
    void Signal();    

};

#endif // PRODUCTSEMAPHORES