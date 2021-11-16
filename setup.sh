set -e

# Run the script from the project root for set up.
export MQ_ROOT=$(pwd)
export GALOIS_HOME=$MQ_ROOT/Galois-2.2.1

# Install required packages.
sudo apt update
sudo apt install build-essential -y
sudo apt install cmake -y
sudo apt install libnuma-dev -y
sudo apt install libpthread-stubs0-dev -y
sudo apt install libboost-all-dev -y
sudo apt install python3.8 -y
sudo apt update


# Compile the project.
chmod +x *.sh
./compile.sh

# Download and prepare graphs.
chmod +x scripts/*.sh
scripts/datasets.sh
