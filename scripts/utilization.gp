set terminal pngcairo size 1024,768 enhanced font 'Verdana,12'

set output 'active_utilization.png'

set title "Active utilization over time"
set xlabel "time"
set ylabel "Active utilization"

# Set the grid
set grid

# Plot data using steps style from a file
plot 'utilization.txt' using 1:2 with steps title 'Active utilization'
