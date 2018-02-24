#NAME: Lawrence Chen
#EMAIL: lawrencechen98@gmail.com
#ID: XXXXXXXXX

default: build

build: lab2_add lab2_list
	
lab2_add: lab2_add.c
	gcc -Wall -Wextra -pthread lab2_add.c -o lab2_add -g

lab2_list: lab2_list.c SortedList.h SortedList.c
	gcc -Wall -Wextra -pthread SortedList.c lab2_list.c -o lab2_list -g

clean:
	rm -rf lab2_add lab2_list lab2a-XXXXXXXXX.tar.gz *.o 

dist: build tests graphs
	tar -cvzf lab2a-XXXXXXXXX.tar.gz lab2_list.c lab2_add.c SortedList.c SortedList.h README Makefile *.gp *.png *.csv tests.sh

tests: build tests.sh
	@chmod +x tests.sh
	-./tests.sh
graphs: tests 
	@chmod +x ./lab2_add.gp
	@chmod +x ./lab2_list.gp
	./lab2_add.gp
	./lab2_list.gp

