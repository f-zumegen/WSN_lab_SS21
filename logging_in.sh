#!/bin/bash

# Bash script to connect to motes, without that shitty gtkterm thing

RED='\033[0;31m'
NC='\033[0m' # No Color

ports=($(ls -d /dev/ttyUSB* 2>/dev/null))

if [ ${#ports[@]} -eq 0 ]
then
	echo -e "${RED}Didn't find any connected motes! Add USB filter (Devices->USB) ${NC}"
else
	echo "Found motes connected on ports:"
	printf '%s\n' "${ports[@]}"
fi


for i in "${ports[@]}"
do
	
	read -p "Log in mote connected to port $i? [y/n] " connect
	# Convert answer to lower case
	if [ "${connect,,}" == "y" ]
	then
		printf "\033c"
		cd ~ && make TARGET=zoul BOARD=remote-revb login PORT=$i
	else
		continue
	fi
done

