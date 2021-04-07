/********************************************
 * bitmapper - Bitmap class
 * This is a special class to create & use
 * bitmaps
 * (c)2021 Brett Huffman
 * 
 * Brett Huffman
 * bitmapper .h file for project
 ********************************************/
#ifndef BITMAPPER
#define BITMAPPER


class bitmapper
{
    private:
        int _size;
        unsigned char* _usageArray;
        
    public:

    bitmapper(int nSize);
    ~bitmapper();
    bitmapper(const bitmapper& oldObj);
    bitmapper& operator=(const bitmapper& rhs);

    // public functions
    void setBitmapBits(int, bool);
    bool getBitmapBits(int);
    void toggleBits(int);
    void debugPrintBits();
    std::string getBitView();
};


#endif // BITMAPPER