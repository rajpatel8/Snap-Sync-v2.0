#!/bin/bash
# setup_terminals.sh
# This script opens five new Terminal windows to run:
#   server_1 (front-end server, run with port 9001)
#   server_2 (PDF server)
#   server_3 (TXT server)
#   server_4 (ZIP server)
#   w25clients (client) connecting to localhost port 9001

# Open server_1 (front-end server)
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\" && ./server_1 9001"
end tell
EOF

sleep 1

# Open server_2 (PDF server)
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\" && ./server_2"
end tell
EOF

sleep 1

# Open server_3 (TXT server)
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\" && ./server_3"
end tell
EOF

sleep 1

# Open server_4 (ZIP server)
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\" && ./server_4"
end tell
EOF

sleep 1

# Open the client and connect to server_1 on localhost:9001
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\" && ./w25clients localhost 9001"
end tell
EOF
