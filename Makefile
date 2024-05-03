# CS 460 Project 2
# Dean Feller 3/3/2024

all:
	gcc -Wall -Werror schedtest.c -o schedtest -lpthread -g


fifo: all
	./schedtest -alg FIFO -input input.txt

sjf: all
	./schedtest -alg SJF -input input.txt

pr: all
	./schedtest -alg PR -input input.txt

run: fifo sjf pr


valfifo: all
	valgrind --leak-check=full --track-origins=yes ./schedtest -alg FIFO -input input.txt

valsjf: all
	valgrind --leak-check=full --track-origins=yes ./schedtest -alg SJF -input input.txt

valpr: all
	valgrind --leak-check=full --track-origins=yes ./schedtest -alg PR -input input.txt

val: valfifo valsjf valpr


clean:
	rm -f schedtest