WSPACE = 	..

CXXFLAGS =	-O3 -g -std=c++11 -Wall -fmessage-length=0 -I$(WSPACE)/minisat -I$(WSPACE)/lingeling -I$(WSPACE)/candy/src -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS

OBJS =		hordesat.o utilities/ClauseFilter.o solvers/MiniSat.o solvers/CandyHorde.o solvers/Lingeling.o utilities/ClauseDatabase.o \
			utilities/SatUtils.o utilities/Logger.o sharing/AllToAllSharingManager.o \
			sharing/LogSharingManager.o

LIBS =		-lz -lpthread -L$(WSPACE)/minisat/build/release/lib -lminisat -L$(WSPACE)/lingeling/ -llgl -L$(WSPACE)/candy/build/ -lcandystatic

TARGET =	hordesat

CXX = 		mpic++

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
