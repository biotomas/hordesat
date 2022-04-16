# get minisat and patch it
if [ ! -d mergesat ]; then
  git clone git@github.com:conp-solutions/mergesat.git
  cd mergesat
  git checkout devel # might be a specific commit lateron
  cd ..

  # fixup MergeSat to provide hook
  # patch minisat/minisat/core/Solver.h < minisat.h.patch
  # patch minisat/minisat/core/Solver.cc < minisat.cc.patch
fi

# make minisat
cd mergesat
make all -j $(nproc)
cd ..

# get lingeling
if [ ! -d lingeling ]; then
  wget http://fmv.jku.at/lingeling/lingeling-ayv-86bf266-140429.zip
  unzip lingeling-ayv-86bf266-140429.zip
  mv *.txt code/
  rm build.sh
  mv code lingeling
fi

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
