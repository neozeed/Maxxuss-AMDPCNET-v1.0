#! /bin/sh
P=/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns
M=MaxxussAMDPCNET.kext
cp -R $M $P
chown -R root:wheel $P/$M
kextcache -k /System/Library/Extensions
