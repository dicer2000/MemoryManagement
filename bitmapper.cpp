/********************************************
 * bitmapper - Bitmap class
 * This is a special class to create & use
 * bitmaps
 * (c)2021 Brett Huffman
 * 
 * Brett Huffman
 * bitmapper .cpp file for project
 ********************************************/

#include <iostream>
#include "bitmapper.h"
#include <string.h>
#include <fstream>
#include <stdlib.h>

using namespace std;

// Constructors / Destructors

bitmapper::bitmapper(int nNumberOfBits)
{
    // Determine the number of characters needed
    int nSize = nNumberOfBits / 8;
    if(nNumberOfBits%8) // If not exactly ^8
        nSize++;
    _size = nSize;
    _usageArray = new unsigned char[nSize];
    memset(_usageArray, '\0', _size); // 0 out array
}

// Destructor
bitmapper::~bitmapper()
{
    delete [] _usageArray;
}

// Copy Constructor
bitmapper::bitmapper(const bitmapper& oldObj)
{
    // Set sizes
    _size = oldObj._size;
    _usageArray = new unsigned char[_size];
    // Copy everything over
    memcpy(_usageArray, oldObj._usageArray, _size);
}

// Assignment Operator
bitmapper& bitmapper::operator=(const bitmapper& rhs)
{
    // Delete any old object and get ready to copy rhs
    delete [] _usageArray;
    _size = rhs._size;
    _usageArray = new unsigned char[_size];
    // Copy everything over
    memcpy(_usageArray, rhs._usageArray, _size);

    return *this;
}

void bitmapper::setBitmapBits(int addr, bool value)
{
    // Check the intput
    if(addr < 0 || addr >= _size*8)
        return;

    if(value)
    {
        // Set the bit at this point in the bitmap
        _usageArray[addr/8] |= (1 << (7 - (addr%8)));
    }
    else
    {
        // Clear the bit
        _usageArray[addr/8] &= ~(1 << (7 - (addr%8)));
    }
}

bool bitmapper::getBitmapBits(int addr)
{
        // Check the intput
    if(addr < 0 || addr >= _size*8)
        return 0;

    // returns true or false based on whether value
    // is set to 1 or 0 in bitmap
    return (_usageArray[addr/8] & (1 << (7 - (addr%8))));
}

void bitmapper::toggleBits(int addr)
{
    // Check the intput
    if(addr < 0 || addr >= _size*8)
        return;

    // Toggle the bit at this point in the bitmap
    _usageArray[addr/8] ^= (1 << (7 - (addr%8)));
}

void bitmapper::debugPrintBits()
{
    // Print a header
    cout << "0         1         2         3         4         5         6         7         " << endl;
    cout << "01234567890123456789012345678901234567890123456789012345678901234567890123456789" << endl;
    // Print the array as bytes
    for (int i = 0; i < _size*8; i++)
        cout << getBitmapBits(i);
    cout << endl;
}

std::string bitmapper::getBitView()
{
    // Print a header
    std::string strRet =  "0         1         2         3         4         5         6         7         ";
    strRet.append("\n");
    strRet.append("01234567890123456789012345678901234567890123456789012345678901234567890123456789");
    strRet.append("\n");
    // Print the array as bytes
    for (int i = 0; i < _size*8; i++)
        strRet.append(getBitmapBits(i) ? "*" : " ");
    strRet.append("\n");
    return strRet;
}

std::string bitmapper::showAsTable(int BitWidth)
{
    if(BitWidth < 1 || _size < 1)
        return "";
    char buffer[3];
    std::string strRet =  "\t";
    int majorMarks = 10;
    int rows = _size*8 / BitWidth;
    for(int i = 0; i < BitWidth; i++)
    {
        if(i%10==0)
            sprintf(buffer, "%d", i/10);
        else
            sprintf(buffer, "%s", " ");
        strRet.append(buffer);
    }
    strRet.append("\n\t");

    for(int i = 0; i < BitWidth; i++)
    {
        sprintf(buffer, "%d", i%10);
        strRet.append(buffer);
    }
    for(int i = 0; i < _size*8; i++)
    {
        if(i%BitWidth==0)
        {
            strRet.append("\n");
            sprintf(buffer, "%d", i/BitWidth);
            strRet.append(buffer);
            strRet.append("\t");
        }
        strRet.append(getBitmapBits(i) ? "1" : "0");
    }

    return strRet;
}