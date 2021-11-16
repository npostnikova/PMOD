mkdir datasets;
cd datasets;
# wget -c http://an.kaist.ac.kr/~haewoon/release/twitter_social_graph/twitter_rv.tar.gz
wget -c https://snap.stanford.edu/data/soc-LiveJournal1.txt.gz
# wget -c https://snap.stanford.edu/data/web-Google.txt.gz
# wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.gr.gz
# wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.co.gz
# wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.gr.gz
# wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.co.gz


# tar xzvf twitter_rv.tar.gz
# gunzip USA-road-d.USA.gr.gz
# gunzip USA-road-d.USA.co.gz
# gunzip USA-road-d.W.gr.gz
# gunzip USA-road-d.W.co.gz
# gunzip soc-LiveJournal1.txt.gz
# gunzip web-Google.txt.gz




echo "Generating weights for unweighted graphs"
python3.7 conv_wel.py soc-LiveJournal1.txt soc-LiveJournal1.wel
python3.7 conv_wel.py web-Google.txt web-Google.wel
python3.7 conv_wel.py twitter_rv.net twitter_rv.wel

echo "Scale road graphs by 64"
python3.7 scale_road.py USA-road-d.USA.gr  USA-road-d.USA64.gr 64
python3.7 scale_road.py USA-road-d.W.gr  USA-road-d.W64.gr 64


echo "Converting graphs to binary format"
$GALOIS_HOME/build_final/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.USA.gr USA-road-dUSA.bin

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.W.gr USA-road-dW.bin

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.USA64.gr USA-road-dUSA64.bin

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.USA64.gr USA-road-dW64.bin


$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -intedgelist2gr twitter_rv.wel twitter40.bin

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -intedgelist2gr web-Google.wel web-Google.bin

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -intedgelist2gr soc-LiveJournal1.wel soc-LiveJournal1.bin

echo "Transpose lj and wg"
$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -gr2tintgr web-Google.bin web-Google.bin_transpose

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -gr2tintgr soc-LiveJournal1.bin soc-LiveJournal1.bin_transpose
