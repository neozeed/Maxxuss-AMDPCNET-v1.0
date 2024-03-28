#! /bin/sh
P=/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns
M=MaxxussAMDPCNET.kext
kextunload $P/$M
sudo rm -r $P/$M
kextcache -k /System/Library/Extensions
