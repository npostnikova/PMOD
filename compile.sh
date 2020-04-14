GALOIS_HOME=./Galois-2.2.1

cd $GALOIS_HOME;
mkdir build;
cd build;
rm -rf *
cmake ../;



#chunk sizes if needed
echo "#define CHUNK_SIZE 64" > $GALOIS_HOME/apps/sssp/chunk_size.h
echo "#define CHUNK_SIZE 64" > $GALOIS_HOME/apps/bfs/chunk_size.h
echo "#define CHUNK_SIZE 64" > $GALOIS_HOME/apps/astar/chunk_size.h
echo "#define CHUNK_SIZE 64" > $GALOIS_HOME/apps/boruvka/chunk_size.h
echo "#define CHUNK_SIZE 64" > $GALOIS_HOME/apps/pagerank/chunk_size.h

cd apps/sssp;
make clean; make;

cd ../bfs;
make clean; make;

cd ../astar;
make clean; make

cd ../boruvka;
make clean; make

cd ../pagerank;
make clean; make

cd ../../tools/graph-convert-standalone/
make clean; make
