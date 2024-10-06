filename = "active_utilization.dat"

# set terminal epslatex standalone color colortext 10
set terminal png
set output "active_utilization.png"
set title "Active Utilization change over time"
set xlabel "Time"
set ylabel "Active Utilization"
set grid
set offsets graph 0, 0, 1, 0
show offsets


set datafile separator whitespace
stats filename using 0 nooutput
set key autotitle columnhead

plot_command = "plot "

do for [i=2:STATS_columns] {
    plot_command = plot_command . sprintf("'%s' using 1:%d with steps lw 3", filename, i)
    if (i < STATS_columns) {
        plot_command = plot_command . ", "
    }
}

eval(plot_command)
