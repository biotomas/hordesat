
# get minisat and patch it
wget https://github.com/niklasso/minisat/archive/master.zip
unzip master.zip
mv minisat-master minisat
patch minisat/minisat/core/Solver.h < minisat.h.patch
patch minisat/minisat/core/Solver.cc < minisat.cc.patch


# make minisat
cd minisat
make
cd ..

# get lingeling
wget http://fmv.jku.at/lingeling/lingeling-ayv-86bf266-140429.zip
unzip lingeling-ayv-86bf266-140429.zip
mv *.txt code/
rm build.sh
mv code lingeling

make lingeling
cd lingeling
./configure.sh
make
cd ..

# get candy 
git submodule init
git submodule update

# make candy
cd candy
git submodule init
git submodule update
mkdir -p build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release ..
make candy
cd ../..


# make hordesat
cd hordesat-src
make
cd ..
mv hordesat-src/hordesat .
