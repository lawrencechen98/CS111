#! /usr/local/cs/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. average wait-for-lock
#
# output:
#	lab2b_1.png ... throughput vs. number of threads for mutex and spin-lock synchronized list operations.
#	lab2b_2.png ... mean time per mutex wait and mean time per operation for mutex-synchronized list operations.
#	lab2b_3.png ... successful iterations vs. threads for each synchronization method.
#	lab2b_4.png ... throughput vs. number of threads for mutex synchronized partitioned lists.
#	lab2b_5.png ... throughput vs. number of threads for spin-lock-synchronized partitioned lists.
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#

# general plot parameters
set terminal png
set datafile separator ","


# throughput vs number of threads for mutex and spin lock
set title "Lab2b-1: Throughput vs. Number of Threads for Each Synchronization Method"
set xlabel "Iterations"
set logscale x 2
set xrange[0.5:]
set ylabel "Throughput (operations/sec)"
set logscale y 10
set key right top
set output 'lab2b_1.png'

# grep out mutex and spin-lock protected runs
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'with Mutex' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'with Spin-lock' with linespoints lc rgb 'blue'


# wait for lock times and per operations times
set title "Lab2b-2: Wait-for-lock Times and Per-operation Times for Mutex-synchronized List Operations"
set xlabel "Threads"
set logscale x 2
set xrange[0.5:]
set ylabel "Mean Time / Operation (ns)"
set logscale y 10
set key left top
set output 'lab2b_2.png'

# grep out mutex protected runs
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
	title 'Completion Time' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
	title 'Wait-for-Lock Time' with linespoints lc rgb 'red'


# how many threads/iterations we can run without failure (w/o yielding)
set title "Lab2b-3: Successful Iterations vs. Threads for Each Synchronization Method"
set xlabel "Threads"
set logscale x 2
set xrange[0.5:]
set ylabel "Successful Iterations"
set logscale y 10
set key left top
set output 'lab2b_3.png'

# grep out unprotected and mutex and spin-lock protected runs
plot \
	 "< grep -e 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'No Protection' with points lc rgb 'red', \
     "< grep -e 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'with Mutex' with points lc rgb 'green', \
     "< grep -e 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'with Spin-lock' with points lc rgb 'blue'


# throughput vs num of threads for mutex
set title "Lab2b-4: Throughput for Mutex Synchronized Partitioned Lists"
set xlabel "Threads"
set logscale x 2
set xrange[0.75:]
set ylabel "Throughput (operations/sec)"
set logscale y 10
set key right top
set output 'lab2b_4.png'

# grep out mutex protected runs
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'orange', \
	 "< grep -e 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'blue', \
	 "< grep -e 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'green'


# throughput vs num of threads for spin lock
set title "Lab2b-5: Throughput for Spin-Lock Synchronized Partitioned Lists"
set xlabel "Threads"
set logscale x 2
set xrange[0.75:]
set ylabel "Throughput (operations/sec)"
set logscale y 10
set key right top
set output 'lab2b_5.png'

# grep out spin-lock protected runs
plot \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'orange', \
	 "< grep -e 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'blue', \
	 "< grep -e 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'green'
