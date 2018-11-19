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
