#!/bin/env bash
sudo umount $NODE
sudo umount "$NODE"1
sudo wipefs -a $NODE
sudo mkdosfs $NODE -n $NAME -F32
sudo rm -rf /tmp/disk
mkdir -p /tmp/disk
sudo mount $NODE /tmp/disk
sudo ./sd_provision.py  -pfile $1 -cfile $2 /tmp/disk/
sudo umount $NODE
