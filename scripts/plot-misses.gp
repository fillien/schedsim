# Set the output file format and name
set terminal png enhanced font 'source sans 3,10'
set output 'misses_util.png'
#set terminal cairolatex eps colortext color transparent size 3.45,2.3
#set output 'misses_util.tex'

# Set the title and labels for the axes
set xlabel "Utilization"
set ylabel "Deadlines misses ratio [\\%]"
set grid
set key box
set key outside top center
set key horizontal

set style fill transparent solid 0.25
set style fill noborder

plot 'data-misses.csv' every ::1 using 1:2 with lines lw 2 lc "purple" title 'GRUB', \
     'data-misses.csv' every ::1 using 1:3 with lines lw 2 lc "blue" title 'PA', \
     'data-misses.csv' every ::1 using 1:4 with lines lw 2 lc "green" title 'FFA', \
     'data-misses.csv' every ::1 using 1:5 with lines lw 2 lc "red" title 'CSF'
