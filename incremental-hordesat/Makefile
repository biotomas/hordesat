WSPACE = 	..

CXXFLAGS =	-O3 -c -g -Wall -fmessage-length=0 -I$(WSPACE)/minisat -I$(WSPACE)/lingeling -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS
#CXXFLAGS =	-O0 -g -Wall -fmessage-length=0 -I$(WSPACE)/minisat -I$(WSPACE)/lingeling -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS

#OBJS =		hordesat.o
#OBJS =		tests/longestpath.o
#OBJS =		tests/genipaessentials.o
OBJS =		tests/dimspec.o\
			HordeLib.o utilities/mympi.o utilities/ClauseFilter.o solvers/MiniSat.o solvers/Lingeling.o solvers/DepQBF.o \
			utilities/ClauseDatabase.o ipahorde.o \
			utilities/BufferManager.o utilities/ClauseManager.o utilities/SatUtils.o utilities/Logger.o \
			sharing/AllToAllSharingManager.o sharing/LogSharingManager.o sharing/AsyncRumorSharingManager.o

LIBS =		-lz -L$(WSPACE)/minisat/build/release/lib -lminisat -L$(WSPACE)/lingeling/ -llgl -L$(WSPACE)/depQBF/baseline-depqbf-version-5.0 -lqdpll -lpthread

#TARGET =	hordesat
TARGET =	dimspec
#TARGET = essentials
#TARGET = incremental

#CXX = 		g++
CXX =		mpic++ 

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
