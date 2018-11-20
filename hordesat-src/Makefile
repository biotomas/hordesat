WSPACE = 	..

CANDYSRC = 	$(WSPACE)/candy/src
CANDYBIN =	$(WSPACE)/candy/build
#CANDYSRC = 	/home/markus/git/candy/src
#CANDYBIN = 	/home/markus/git/candy/build-debug
MINISRC = 	$(WSPACE)/minisat
MINIBIN = 	$(WSPACE)/minisat/build/release/lib
LGLSRC = 	$(WSPACE)/lingeling
LGLBIN = 	$(WSPACE)/lingeling

CXXFLAGS =	-O3 -g -std=c++11 -Wall -fmessage-length=0 -I$(MINISRC) -I$(LGLSRC) -I$(CANDYSRC) -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS

OBJS =		hordesat.o utilities/ClauseFilter.o solvers/MiniSat.o solvers/CandyHorde.o solvers/Lingeling.o utilities/ClauseDatabase.o \
			utilities/SatUtils.o utilities/Logger.o sharing/AllToAllSharingManager.o sharing/LogSharingManager.o

LIBS =		-lz -lpthread -L$(MINIBIN) -lminisat -L$(LGLBIN) -llgl -L$(CANDYBIN) -lcandystatic

TARGET =	hordesat

CXX = 		mpic++

$(TARGET):	$(OBJS)
		$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:		$(TARGET)

clean:	rm -f $(OBJS) $(TARGET)
