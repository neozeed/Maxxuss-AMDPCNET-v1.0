#! /bin/sh
M=MaxxussAMDPCNET.kext
chown -R root:wheel $M
kextload $M
