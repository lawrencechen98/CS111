#NAME: Lawrence Chen
#EMAIL: lawrencechen98@gmail.com
#ID: XXXXXXXXX

default: build

build: lab4b

lab4b: lab4b.c
	gcc -Wall -Wextra -std=gnu99 -g -lmraa -lm -o lab4b lab4b.c

clean:
	rm -f lab4b-XXXXXXXXX.tar.gz lab4b test.txt

dist: lab4b
	tar -cvzf lab4b-XXXXXXXXX.tar.gz lab4b.c Makefile README

check: lab4b
	echo "SCALE=F\nPERIOD=0\nSCALE=C\nSTART\nSTART\nSTOP\nSTOP\nSTART\nLOG test\nLOG test\nLOG test\nOFF" | ./lab4b --period=2 --scale=C --log="test.txt"; \
	EXIT_CODE=$$?;\
    echo "command exited with $$EXIT_CODE";\
    exit $$EXIT_CODE
