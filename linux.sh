#!/bin/bash
# setup_terminals.sh for Linux (using GNOME Terminal)
# This script opens five new GNOME Terminal windows to run:
#   server_1 (front-end server, run with port 9001)
#   server_2 (PDF server)
#   server_3 (TXT server)
#   server_4 (ZIP server)
#   w25clients (client) connecting to localhost port 9001

# Open server_1 (front-end server)
gnome-terminal -- bash -c "cd \"$(pwd)\" && ./server_1 9001; exec bash"
sleep 1

# Open server_2 (PDF server)
gnome-terminal -- bash -c "cd \"$(pwd)\" && ./server_2; exec bash"
sleep 1

# Open server_3 (TXT server)
gnome-terminal -- bash -c "cd \"$(pwd)\" && ./server_3; exec bash"
sleep 1

# Open server_4 (ZIP server)
gnome-terminal -- bash -c "cd \"$(pwd)\" && ./server_4; exec bash"
sleep 1

# Open the client (w25clients) connecting to server_1 (localhost:9001)
gnome-terminal -- bash -c "cd \"$(pwd)\" && ./w25clients localhost 9001; exec bash"
