# Set the output file format and name
set terminal pdfcairo enhanced font 'arial,10'
set output 'data_plot.pdf'

# Set the title and labels for the axes
set title "Energy Consumption vs. Utilization"
set xlabel "Utilization"
set ylabel "Total Energy Consumption"

set style fill transparent solid 0.25
set style fill noborder

plot 'data.csv' every ::1 using 1:2 with lines lc "blue" title 'pa', \
     'data.csv' every ::1 using 1:3:4 with filledcurves lc "blue" notitle, \
     'data.csv' every ::1 using 1:5 with lines lc "green" title 'pa_f_min', \
     'data.csv' every ::1 using 1:6:7 with filledcurves lc "green" notitle
