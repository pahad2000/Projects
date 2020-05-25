xterm -hold -title "Peer 1" -e "`pwd`/cdht 1 3 4 300 0.3" &
xterm -hold -title "Peer 3" -e "`pwd`/cdht 3 4 1 300 0.3" &
xterm -hold -title "Peer 4" -e "`pwd`/cdht 4 1 3 300 0.3" &
