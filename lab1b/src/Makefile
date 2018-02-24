#NAME: Lawrence Chen
#EMAIL: lawrencechen98@gmail.com
#ID: XXXXXXXXX

.SILENT:

default: server client

server:
	gcc -Wall -Wextra lab1b-server.c -o lab1b-server -lz

client:
	gcc -Wall -Wextra lab1b-client.c -o lab1b-client -lz

dist:
	tar -czvf lab1b-XXXXXXXXX.tar.gz lab1b-server.c lab1b-client.c README Makefile  

clean: 
	rm -f lab1b-server lab1b-client lab1b-XXXXXXXXX.tar.gz 
