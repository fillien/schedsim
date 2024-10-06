# Set the output file format and name
#set terminal png enhanced font 'source sans 3,10'
#set output 'energy_util.png'
set terminal cairolatex eps colortext color transparent size 3.45,2.3
set output output_file

# Set the title and labels for the axes
set xlabel "Active utilization"
set ylabel "Power Consumption [W]"
set grid
set key box
set key outside top center
set key horizontal

set style fill transparent solid 0.25
set style fill noborder

plot input_file every ::1 using 1:2 with linespoints lw 3 lc "purple" title 'GRUB', \
     input_file every ::1 using 1:5 with linespoints lw 3 lc "blue" title 'PA', \
     input_file every ::1 using 1:8 with linespoints lw 3 lc "green" title 'FFA', \
     input_file every ::1 using 1:11 with linespoints lw 3 lc "red" title 'CSF', \
