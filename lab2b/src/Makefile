#NAME: Lawrence Chen
#EMAIL: lawrencechen98@gmail.com
#ID: XXXXXXXXX

default: build

build: lab2_list

lab2_list: lab2_list.c SortedList.h SortedList.c
	gcc -Wall -Wextra -pthread -L /lib64 -lprofiler SortedList.c lab2_list.c -o lab2_list -g

clean:
	rm -rf lab2_list lab2b-XXXXXXXXX.tar.gz *.o 

dist: build tests graphs profile
	tar -cvzf lab2b-XXXXXXXXX.tar.gz lab2_list.c SortedList.c SortedList.h README Makefile lab2_list.gp *.png lab2b_list.csv tests.sh profile.out

profile: build
	-rm -f ./raw.gperf profile.out
	LD_PRELOAD=/lib64/libprofiler.so CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	@pprof --text ./lab2_list ./raw.gperf > profile.out
	@pprof --list=list_thread ./lab2_list ./raw.gperf >> profile.out
	-rm -f ./raw.gperf

tests: build tests.sh
	rm -rf *.csv
	@chmod +x tests.sh
	-./tests.sh

graphs: tests 
	@chmod +x ./lab2_list.gp
	./lab2_list.gp

