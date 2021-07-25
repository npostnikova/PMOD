# Download and prepare datasets for experiments.
set -e

mkdir -p $MQ_ROOT/datasets
cd $MQ_ROOT/datasets

##### DOWNLOAD ######
# Road graphs
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.gr.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.co.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.gr.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.W.co.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.CTR.gr.gz
wget -c http://users.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.CTR.co.gz

# Social graphs
wget -c https://snap.stanford.edu/data/soc-LiveJournal1.txt.gz
wget -c https://suitesparse-collection-website.herokuapp.com/MM/GAP/GAP-twitter.tar.gz
# TODO web





