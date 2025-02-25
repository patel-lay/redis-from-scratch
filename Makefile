# CXX = g++
# CXXLAGS = -std=c++23 -Wall -g

# server: hashtable.o server.o 
# 	$(CXX) $(CXXLAGS) -o server hashtable.o server.o
 
#  %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) -c -o $@ $<
# # server.o: server.cpp hashtable.h 
# #     $(CC) $(CFLAGS) -c server.cpp
 
# # hashtable.o: hashtable.h
 
# # # Square.o: Square.h Point.h
# # clean:
# # 	rm -f *.o all


CXX = clang++ # or clang++
CXXFLAGS = -Wall -g -std=c++23 # or another C++ standard like c++11, c++14, c++20
#CFLAGS = -Wall -g


# LDFLAGS = -lstdc++

TARGET = server
SOURCES = server.cpp hashtable.cpp
OBJECTS = $(SOURCES:.cpp=.o) 

all: $(TARGET) client

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET) 


client: client.cpp
	$(CXX) -Wall $(CXXFLAGS) -g -o client client.cpp

# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS) client client.o