# Set the output file format and name
#set terminal pdfcairo enhanced font 'source sans 3,10'
set terminal epslatex standalone color colortext 10
set output 'energy_util.tex'

# Set the title and labels for the axes
set title "Energy Consumption vs. Utilization"
set xlabel "Utilization"
set ylabel "Total Energy Consumption"
set grid

set style fill transparent solid 0.25
set style fill noborder

plot 'data.csv' every ::1 using 1:2 with lines lc "blue" title 'pa', \
     'data.csv' every ::1 using 1:3:4 with filledcurves lc "blue" notitle, \
     'data.csv' every ::1 using 1:5 with lines lc "green" title 'pa\_f\_min', \
     'data.csv' every ::1 using 1:6:7 with filledcurves lc "green" notitle, \
     'data.csv' every ::1 using 1:8 with lines lc "red" title 'pa\_m\_min', \
     'data.csv' every ::1 using 1:9:10 with filledcurves lc "red" notitle
