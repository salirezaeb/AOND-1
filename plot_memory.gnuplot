set datafile separator ','
set terminal png size 900,600
set output 'plot_memory.png'
set title 'Estimated Memory Usage vs Stride'
set xlabel 'Stride (bits)'
set ylabel 'Estimated Memory (bytes)'
set grid
set style data histograms
set style fill solid 1.0 border -1
set boxwidth 0.7
plot 'summary.csv' using 1:11 every ::1 title 'Mem(bytes)' with boxes
