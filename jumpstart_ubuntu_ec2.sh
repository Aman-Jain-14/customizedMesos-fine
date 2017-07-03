#!/bin/bash

if [ $(whoami) != "root" ]; then
	echo "Please make sure you are in root mode!"
else
	hostname=$(cat /etc/hostname)
	hostip=${hostname#"ip-"}
	hostip=${hostip//"-"/"."}
	printf "\n$hostip $hostname" >> /etc/hosts
	apt-get update
	apt-get install -y openjdk-8-jre-headless
	apt-get install -y openjdk-8-jdk-headless
	apt-get install -y autoconf libtool
	apt-get -y install build-essential python-dev libcurl4-nss-dev libsasl2-dev libsasl2-modules maven libapr1-dev libsvn-dev
	apt-get -y install lib32z1-dev
	./bootstrap
	mkdir build
	cd build
	../configure
	make
	make install
	# echo "next step: after changing /etc/hosts, do <make check> to generate test programs"
	make check
	ln src/.libs/libmesos-1.3.0.so /lib/.
fi
