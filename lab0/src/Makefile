#NAME: Lawrence Chen
#EMAIL: lawrencechen98@gmail.com
#ID: XXXXXXXXX

.SILENT:

default:
	gcc -Wall -Wextra -o lab0 -g lab0.c

dist:
	tar -czvf lab0-XXXXXXXXX.tar.gz lab0.c README Makefile breakpoint.png backtrace.png  

clean: 
	rm -f lab0 test.txt output.txt read_only.txt lab0-XXXXXXXXX.tar.gz 

check: default testReadWrite testInvalidOptions testInvalidInput testInvalidOutput testSegfaultCatch

testReadWrite:
	echo "test" > test.txt 
	./lab0 --input=test.txt --output=output.txt  
	if [ $$? -eq 0 ]; then \
	echo "Checking return code with --input and --output flags is 0 ... OK"; \
	else \
	echo "Checking return code with --input and --output flags is 0 ... FAILURE"; \
	fi; \
	cmp -s test.txt output.txt; \
	if [ $$? -eq 0 ]; then \
	echo "Checking that results are expected with correct read and write ... OK"; \
	else \
	echo "Checking that results are expected with correct read and write ... FAILURE"; \
	fi; \
	rm -f test.txt output.txt

testInvalidOptions:
	./lab0 --invalid; \
	if [ $$? -eq 1 ]; then \
	echo "Checking return code of invalid options is 1 ... OK"; \
	else \
	echo "Checking return code of invalid options is 1 ... FAILURE"; \
	fi

testInvalidInput:
	./lab0 --input=test.txt; \
	if [ $$? -eq 2 ]; then \
	echo "Checking return code with invalid --input file is 2 ... OK"; \
	else \
	echo "Checking return code with invalid --input file is 2 ... FAILURE"; \
	fi

testInvalidOutput:
	touch read_only.txt ; \
	chmod 444 read_only.txt ; \
	./lab0 --output=read_only.txt; \
	if [ $$? -eq 3 ]; then \
	echo "Checking return code with invalid --output file is 3 ... OK"; \
	else \
	echo "Checking return code with invalid --output file is 3 ... FAILURE"; \
	fi

testSegfaultCatch:
	./lab0 --segfault --catch; \
	if [ $$? -eq 4 ]; then \
	echo "Checking return code when segmentation fault is caught is 4 ... OK"; \
	else \
	echo "Checking return code when segmentation fault is caught is 4 ... FAILURE"; \
	fi; \
	rm -f read_only.txt
