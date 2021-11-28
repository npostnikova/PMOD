# Download and prepare datasets for experiments.
set -e

if [ -z $MQ_ROOT || -z $GALOIS_HOME ]
then
  echo "Please set MQ_ROOT and GALOIS_HOME env variables"
  exit 1
fi

mkdir -p $MQ_ROOT/datasets
cd $MQ_ROOT/datasets

##### DOWNLOAD ######
echo "Downloading graphs"
# Road graphs
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.gr.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.co.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.gr.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.co.gz
#wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.CTR.gr.gz
#wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.CTR.co.gz

# Social graphs
#wget -c https://snap.stanford.edu/data/soc-LiveJournal1.txt.gz
wget -c https://suitesparse-collection-website.herokuapp.com/MM/GAP/GAP-twitter.tar.gz
wget -c https://suitesparse-collection-website.herokuapp.com/MM/GAP/GAP-web.tar.gz

gunzip USA-road-d.USA.gr.gz
gunzip USA-road-d.USA.co.gz
gunzip USA-road-d.W.gr.gz
gunzip USA-road-d.W.co.gz
#gunzip USA-road-d.CTR.gr.gz
#gunzip USA-road-d.CTR.co.gz
#gunzip soc-LiveJournal1.txt.gz
tar -xvzf GAP-twitter.tar.gz
tar -xvzf GAP-web.tar.gz


##### CONVERT TO BINARY GR #####
echo "Converting graphs"

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.USA.gr USA-road-dUSA.bin
$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.W.gr USA-road-dW.bin
#$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -dimacs2gr USA-road-d.CTR.gr USA-road-dCTR.bin

rm USA-road-d.USA.gr
rm USA-road-d.W.gr
#rm USA-road-d.CTR.gr

$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -mtx2intgr GAP-twitter/GAP-twitter.mtx GAP-twitter.bin
$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -mtx2intgr GAP-web/GAP-web.mtx GAP-web.bin
#$GALOIS_HOME/build/tools/graph-convert-standalone/graph-convert-standalone -edgelist2randgr soc-LiveJournal1.txt soc-LiveJournal1.bin

echo "Cleaning"

rm GAP-twitter.tar.gz
rm GAP-web.tar.gz
rm -R GAP-twitter
rm -R GAP-web
#rm soc-LiveJournal1.txt

echo "Graphs are prepared"


