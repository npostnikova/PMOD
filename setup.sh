set -e

# Run the script from the project root for set up.
export MQ_ROOT=$(pwd)
export GALOIS_HOME=$MQ_ROOT/Galois-2.2.1

# Install required packages.
sudo apt-get update
sudo apt-get install build-essential -y
sudo apt-get install cmake -y
sudo apt-get install libnuma-dev -y
sudo apt-get install libpthread-stubs0-dev -y
sudo apt-get install libboost-all-dev -y
sudo apt-get install python3.8 -y
sudo apt-get install python3-pip             -y
sudo apt-get install wget -y
sudo apt-get update

pip install 'matplotlib<3.5'
pip install seaborn
pip install numpy

# Update the configuration if needed.
nano $MQ_ROOT/set_envs.sh

# Compile the project.
chmod +x *.sh
$MQ_ROOT/compile.sh

chmod +x $MQ_ROOT/scripts/*.sh
chmod +x $MQ_ROOT/scripts/build/*.sh
chmod +x $MQ_ROOT/scripts/single_run/*.sh

# Download and prepare graphs.
$MQ_ROOT/scripts/datasets.sh

# Check that everything is set up
$MQ_ROOT/scripts/verify_setup.sh
