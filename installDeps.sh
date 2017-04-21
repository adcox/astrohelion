#!/bin/sh

# Install dependencies for Astrohelion on a Linux 64-bit machine
# All included files, libraries, and executables will be placed in
# the directory specified by INSTALL_DIR

LOG="installLog.log"
INSTALL_DIR="/opt/local"

# Figure out the OS
OS="unknown"
unamestr=`uname`
if [[ "$unamestr" == "Linux" ]]; then
	OS="linux"
elif [[ "$unamestr" == "Darwin" ]]; then
	OS="darwin"
fi

mkdir -p deps
cd deps

## Boost
wget https://sourceforge.net/projects/boost/files/boost/1.62.0/boost_1_62_0.tar.gz
tar -xvzf boost_1_62_0.tar.gz >> $LOG
cd boost_1_62_0
echo "Installing BOOST"
./bootstrap.sh --prefix=$INSTALL_DIR >> $LOG
if [[ "$OS" == "darwin" ]]; then
	./b2 toolset=gcc-6 -d0 install --with-filesystem --with-system --with-test >> $LOG
else
	./b2 -d0 install --with-filesystem --with-system --with-test >> $LOG
fi
cd ..
rm boost_1_62_0.tar.gz

## CSpice
if [[ "$OS" == "darwin" ]]; then
	wget http://naif.jpl.nasa.gov/pub/naif/toolkit/C/MacIntel_OSX_AppleC_64bit/packages/cspice.tar.Z
else 
	wget http://naif.jpl.nasa.gov/pub/naif/toolkit/C/PC_Linux_GCC_64bit/packages/cspice.tar.Z
fi
tar -xvzf cspice.tar.Z >> $LOG
cd cspice
echo "Installing CSPICE"
mkdir -p $INSTALL_DIR/include/cspice
mkdir -p $INSTALL_DIR/bin/cspice
cp -R include/* $INSTALL_DIR/include/cspice/
mv lib/cspice.a $INSTALL_DIR/lib/libcspice.a
mv lib/csupport.a $INSTALL_DIR/lib/libcsupport.a
mv exe/* $INSTALL_DIR/bin/cspice/
cd ..
rm cspice.tar.Z

## MatIO
wget https://sourceforge.net/projects/matio/files/matio-1.5.10.tar.gz
tar -xvzf matio-* >> $LOG
cd matio-*
echo "Installing MATIO"
./configure --prefix=$INSTALL_DIR >> $LOG
make >> $LOG
make install >> $LOG
cd ..
rm matio-1.5.10.tar.gz

## GSL
wget http://mirror.nexcess.net/gnu/gsl/gsl-2.2.1.tar.gz
tar -xvzf gsl-2.2.1.tar.gz >> $LOG
cd gsl-2.2.1
echo "Installing GSL"
./configure --prefix=$INSTALL_DIR >> $LOG
make >> $LOG
make install >> $LOG
cd ..
rm gsl-2.2.1.tar.gz

if [[ "$OS" == "linux" ]]; then
	libtool --finish $INSTALL_DIR/lib/
fi

## Download and move SPICE data
wget http://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de430.bsp
wget http://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls
mkdir -p ~/.config/astrohelion
mv de430.bsp data/data_SPICE/
mv naif0012.tls data/data_SPICE/

## Eigen - Header only
wget http://bitbucket.org/eigen/eigen/get/3.3.3.tar.gz
tar -xvzf 3.3.3.tar.gz
mv eigen* eigen
mv eigen/Eigen $INSTALL_DIR/include/Eigen
mv eigen/unsupported $INSTALL_DIR/include/Eigen/unsupported
rm 3.3.3.tar.gz

## Configuration Files
# cp ../travis_settings.xml ~/.config/astrohelion/user_settings.xml