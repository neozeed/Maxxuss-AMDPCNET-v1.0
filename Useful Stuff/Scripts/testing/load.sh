#! /bin/sh
D=~/Projects/MaxxussPCNET/build/Deployment
M=MaxxussAMDPCNET.kext
cd $D
mkdir test
cd test
rm -r *
cd ..
cp -R $M test/
cd test
sudo chown -R root:wheel $M
sudo kextload $M
