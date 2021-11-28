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

#Create Experiments.h files.
echo "" > $GALOIS_HOME/apps/sssp/Experiments.h
echo "" > $GALOIS_HOME/apps/bfs/Experiments.h
echo "" > $GALOIS_HOME/apps/astar/Experiments.h
echo "" > $GALOIS_HOME/apps/boruvka/Experiments.h

