#! /bin/sh
D=~/Projects/MaxxussPCNET/build/Deployment
M=MaxxussAMDPCNET.kext
rm -r $M/*
mkdir $M
cp -R $D/$M .
rm $M/Contents/pbdevelopment.plist
strip -SXx $M/Contents/MacOS/MaxxussAMDPCNET
chown -R root:wheel $M
#kextload $M
