#!/bin/bash

# I wrote this script to make my life easier.
# Will probably not work if you don't change the path :)
# And the hardcoded file names :)

cd /media/sf_WSN_Lab/Project/group4/Project

SESSION_NAME="routing"
tmux has-session -t $SESSION_NAME &> /dev/null

if [ $? != 0 ] 
 then
	tmux new-session -n "DEV" -s $SESSION_NAME -d
	tmux split-window -h -t $SESSION_NAME
	tmux send-keys -t $SESSION_NAME "vim routing.c" C-m
	tmux new-window  -n "DEBUG" -t $SESSION_NAME:
	tmux send-keys -t $SESSION_NAME C-m "./logging_in.sh"
	tmux split-window -v -t $SESSION_NAME
	tmux send-keys -t $SESSION_NAME C-m "./logging_in.sh"
	tmux new-window  -n "BUILD" -t $SESSION_NAME:
	tmux send-keys -t $SESSION_NAME C-m "./flasher.sh"
	tmux new-window  -n "DOXYGEN" -t $SESSION_NAME:
	tmux send-keys -t $SESSION_NAME "cd Documentation; doxygen Doxygen.cfg" C-m
	tmux new-window  -n "PROJ-CONF" -t $SESSION_NAME:
	tmux send-keys -t $SESSION_NAME "vim project-conf.h" C-m
fi

tmux attach -t $SESSION_NAME
