set terminal pngcairo size 1024,768 enhanced font 'Verdana,12'

set output 'frequency.png'

set title "Frequency over time"
set xlabel "time"
set ylabel "frequency"

# Set the grid
set grid

# Plot data using steps style from a file
plot 'frequency.txt' using 1:2 with steps title 'frequency'
