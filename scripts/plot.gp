# Set the output file format and name
set terminal png enhanced font 'source sans 3,10'
set output 'energy_util.png'
#set terminal cairolatex eps colortext color transparent size 3.45,2.3
#set output 'energy_util.tex'

# Set the title and labels for the axes
set xlabel "Utilization"
set ylabel "Power Consumption [W]"
set grid
set key box
set key outside top center
set key horizontal

set style fill transparent solid 0.25
set style fill noborder

plot 'data-energy.csv' every ::1 using 1:2 with lines lc "purple" title 'grub', \
     'data-energy.csv' every ::1 using 1:3:4 with filledcurves lc "purple" notitle, \
     'data-energy.csv' every ::1 using 1:5 with lines lc "blue" title 'pa', \
     'data-energy.csv' every ::1 using 1:6:7 with filledcurves lc "blue" notitle, \
     'data-energy.csv' every ::1 using 1:8 with lines lc "green" title 'pa\_f\_min', \
     'data-energy.csv' every ::1 using 1:9:10 with filledcurves lc "green" notitle, \
     'data-energy.csv' every ::1 using 1:11 with lines lc "red" title 'pa\_m\_min', \
     'data-energy.csv' every ::1 using 1:12:13 with filledcurves lc "red" notitle
