#!/bin/bash

# Clean and build the project
make clean
make all

# Open three different terminals and run the servers
gnome-terminal -- bash -c "./naming_server; exec bash"
sleep 1
gnome-terminal -- bash -c "./storage_server 8080 Test/SS1; exec bash"
gnome-terminal -- bash -c "./storage_server 7070 Test/SS2; exec bash"
gnome-terminal -- bash -c "./client; exec bash"
