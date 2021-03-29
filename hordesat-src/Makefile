WSPACE = 	..

#CANDYSRC = 	$(WSPACE)/candy/src
#CANDYBIN =	$(WSPACE)/candy/build
#CANDYSRC = 	/home/markus/git/candy/src
#CANDYBIN = 	/home/markus/git/candy/build-debug
MINISRC = 	$(WSPACE)/mergesat
MINIBIN = 	$(WSPACE)/mergesat/build/release/lib
#LGLSRC = 	$(WSPACE)/lingeling
#LGLBIN = 	$(WSPACE)/lingeling

COPTIMIZE := -O3
CXXFLAGS =	$(COPTIMIZE) -g -std=c++11 -Wall -fmessage-length=0 -I$(MINISRC) -I$(LGLSRC) -I$(CANDYSRC) -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS


OBJS =		hordesat.o utilities/ClauseFilter.o solvers/MergeSat.o utilities/ClauseDatabase.o \
			utilities/SatUtils.o utilities/Logger.o sharing/AllToAllSharingManager.o sharing/LogSharingManager.o
#CXXFLAGS += -DUSE_CANDY
#OBJS +=	    solvers/CandyHorde.o
#CXXFLAGS += -DUSE_LGL
#OBJS +=	    solvers/Lingeling.o

LIBS =		-L$(MINIBIN) -lmergesat
# LIBS +=     -L$(LGLBIN) -llgl
# LIBS +=     -L$(CANDYBIN) -lcandylib
LIBS +=     -lz -lpthread


hordesat: $(WSPACE)/mergesat/build/release/lib/libmergesat.a

$(WSPACE)/mergesat/build/release/lib/libmergesat.a:
		$(MAKE) -C $(WSPACE)/mergesat lr ld

TARGET =	hordesat

CXX = 		mpic++

$(TARGET):	$(OBJS)
		$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:		$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
