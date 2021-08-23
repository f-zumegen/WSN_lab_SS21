#!/bin/bash

# Bash script to flash multiple Motes with different IDs

RED='\033[0;31m'
NC='\033[0m' # No Color

BINARY=routing.upload

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
	
	read -p "Flash port $i? [y/n] " flash
	# Convert answer to lower case
	if [ "${flash,,}" == "y" ]
	then
		read -p "Node id? " node_id
		echo -e "${RED}Flashing port $i with node id $node_id${NC}"
		make TARGET=zoul BOARD=remote-revb NODEID=0x$node_id PORT=$i $BINARY
	else
		continue
	fi
done

