
BOOST_INCLUDES = /usr/include
BOOST_LIBS = /usr/lib64

all: example

example: example.cpp
	g++ -std=c++11 -I../libs -I$(BOOST_INCLUDES) -L$(BOOST_LIBS) -pthread -lboost_thread -lboost_system example.cpp -o example 


