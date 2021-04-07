# Improved Makefile by Brett Huffman v1.4
# (c)2021 Brett Huffman
# This includes 2 executables, sos and user_proc

# App 1 - builds the oss program
appname1 := oss
srcfiles := $(shell find . -name "oss*.cpp") ./productSemaphores.cpp ./bitmapper.cpp

# For debugging
#$(error   VAR is $(srcfiles))
#CXXFLAGS := -Wpadded
objects1  := $(patsubst %.cpp, %.o, $(srcfiles))

all: $(appname1)

$(appname1): $(objects1) $(LDLIBS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname1) $(objects1) $(LDLIBS)

# App 2 - builds the child program
appname2 := user_proc
srcfiles := $(shell find . -name "user_proc*.cpp") ./productSemaphores.cpp
objects2  := $(patsubst %.cpp, %.o, $(srcfiles))

all: $(appname2)

$(appname2): $(objects2)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname2) $(objects2) $(LDLIBS)


clean:
	rm -f $(objects1)
	rm -f $(appname1)
	rm -f $(objects2)
	rm -f $(appname2)
	rm -f logfile*