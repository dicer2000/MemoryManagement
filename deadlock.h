/********************************************
 * deadlock.h - An inline implementation of
 * the deadlock algorithm as discussed in
 * deadlock lectures.
 * 
 * Brett Huffman
 * CMP SCI 4760
 * deadlock.h file for project
 ********************************************/
#ifndef DEADLOCK_H
#define DEADLOCK_H


// Check if the request for process pnum is less than or equal to available
// vector
bool req_lt_avail ( const int * req, const int * avail, const int pnum,
    const int num_res )
{
    int i ( 0 );
    for ( ; i < num_res; i++ )
        if ( req[pnum*num_res+i] > avail[i] )
            break;
    return ( i == num_res );
}

// Deadlock process - modified from the original to return one of the
// deadlocked processes should one be detected
int deadlock ( const int * available, const int m, const int n,
    const int * request, const int * allocated )
{
    int work[m]; // m resources
    bool finish[n]; // n processes
    for ( int i ( 0 ); i < m; )
    {
        work[i] = available[i]; // Not sure if this is right => Test
        i++;
    }
    
    for ( int i ( 0 ); i < n; finish[i++] = false );
    
    int p ( 0 );
    for ( ; p < n; p++ ) // For each process
    {
        if ( finish[p] ) continue;
        if ( req_lt_avail ( request, work, p, m ) )
        {
            finish[p] = true;
            for ( int i ( 0 ); i < m; i++ )
                work[i] += allocated[p*m+i];
            p = -1;
        }
    }
    for ( p = 0; p < n; p++ )
        if ( ! finish[p] )
            break;

//    return ( p != n );  // True is a deadlock

    return ( p == n ? -1 : p);   // -1 is no deadlock, otherwise send a process
                                // in deadlock that should have a resource released
}

#endif // DEADLOCK_H