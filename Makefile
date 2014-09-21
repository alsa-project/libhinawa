all:
	if [ -e core ]; then rm core; fi
	if [ -e /tmp/test ]; then rm /tmp/test; fi
	gcc -g -Wall ./src/reactor.c ./src/fcp.c ./src/fw.c ./src/efw.c ./src/snd.c -lpthread -lasound ./test.c -o /tmp/test
