all: zfs-util

CXXFLAGS=-Wall -g

zfs-util: zfs-util.cpp

dbgmsg-tail: dbgmsg-tail.cpp

install: zfs-util dbgmsg-tail
	mkdir -pv ${out}/bin
	cp zfs-util dbgmsg-tail ${out}/bin/
