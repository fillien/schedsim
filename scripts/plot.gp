# Set the output file format and name
set terminal pngcairo enhanced font 'arial,10'
set output 'data_plot.png'

# Set the title and labels for the axes
set title "Energy Consumption vs. Utilization"
set xlabel "Utilization"
set ylabel "Energy"

# Set the range for the axes
set xrange [0:3.1]
set yrange [0:4500]

# Plot the first line graph (energy_x)
plot 'data_merged.csv' every ::1 using 1:2 with lines title 'energy_x', \
     'data_merged.csv' every ::1 using 1:3 with lines title 'energy_y'
