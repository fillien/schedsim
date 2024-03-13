# Create a script to plot the data
set term pngcairo
set output 'plot_data.png'

# Plot the data points
plot 'data' using 1:2 with lines title 'Data Points'

set output

