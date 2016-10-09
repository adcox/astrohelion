#!/bin/sh

# Install Dependencies for Travis CI system
mkdir deps
cd deps

sudo apt-get -qq update
sudo apt-get install gsl matio

# Boost
wget https://sourceforge.net/projects/boost/files/boost/1.62.0/boost_1_62_0.tar.gz
tar -xvzf boost_1_62_0.tar.gz
cd boost_1_62_0 && ./boostrap.sh && sudo ./b2 install
cd ..

# CSpice
wget http://naif.jpl.nasa.gov/pub/naif/toolkit//C/PC_Linux_GCC_64bit/packages/cspice.tar.Z
tar -xvzf cspice.tar.Z
cd cspice
sudo cp -R include/* /usr/local/include/cspice/
sudo mv lib/cspice.a /usr/local/lib/libcspice.a
sudo mv lib/csupport.a /usr/local/lib/libcsupport.a
sudo cp exe/* /usr/local/bin/cspice/
cd ..

# MatIO
wget https://sourceforge.net/projects/matio/files/matio/1.5.3/matio-1.5.3.tar.gz
tar -xvzf matio-1.5.3.tar.gz
cd matio-1.5.3
./configure && make && sudo make install
cd ..

# GSL
wget http://mirror.nexcess.net/gnu/gsl/gsl-2.1.tar.gz
tar -xvzf gsl-2.1.tar.gz
cd gsl-2.1
./configure && make && sudo make install
cd ..

rm *.tar.*