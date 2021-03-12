WSPACE = 	..

CANDYSRC = 	$(WSPACE)/candy/src
CANDYBIN =	$(WSPACE)/candy/build
#CANDYSRC = 	/home/markus/git/candy/src
#CANDYBIN = 	/home/markus/git/candy/build-debug
MINISRC = 	$(WSPACE)/mergesat
MINIBIN = 	$(WSPACE)/mergesat/build/release/lib
LGLSRC = 	$(WSPACE)/lingeling
LGLBIN = 	$(WSPACE)/lingeling

COPTIMIZE := -O3
CXXFLAGS =	$(COPTIMIZE) -g -std=c++11 -Wall -fmessage-length=0 -I$(MINISRC) -I$(LGLSRC) -I$(CANDYSRC) -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS


OBJS =		hordesat.o utilities/ClauseFilter.o solvers/MergeSat.o solvers/CandyHorde.o solvers/Lingeling.o utilities/ClauseDatabase.o \
			utilities/SatUtils.o utilities/Logger.o sharing/AllToAllSharingManager.o sharing/LogSharingManager.o

LIBS =		-L$(MINIBIN) -lmergesat -L$(LGLBIN) -llgl -L$(CANDYBIN) -lcandylib -lz -lpthread

TARGET =	hordesat

CXX = 		mpic++

$(TARGET):	$(OBJS)
		$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:		$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
