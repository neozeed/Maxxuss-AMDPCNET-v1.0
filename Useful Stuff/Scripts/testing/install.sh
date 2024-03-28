#! /bin/sh
P=/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns
D=~/Projects/MaxxussPCNET/build/Deployment
M=MaxxussAMDPCNET.kext
sudo rm -r $P/$M
cp -R $D/$M $P
sudo chown -R root:wheel $P/$M
kextcache -k /System/Library/Extensions
