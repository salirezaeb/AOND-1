set datafile separator ','
set terminal png size 900,600
set output 'plot_time.png'
set title 'Average Lookup Time vs Stride'
set xlabel 'Stride (bits)'
set ylabel 'Average Lookup Time (ns)'
set grid
set style data histograms
set style fill solid 1.0 border -1
set boxwidth 0.7
plot 'summary.csv' using 1:9 every ::1 title 'Avg(ns)' with boxes
