# CS 460 Operating Systems Project 2 - CPU Scheduling
Dean Feller 3/3/2024

## Schedtest
Simulates the CPU and I/O burst times of processes using different scheduling algorithms to measure the performance of those algorithms. The algorithms being tested are First Come First Serve (FCFS, or FIFO), Shortest Job First (SJF), and Priority (PR).

## Build Instructions
In the Makefile are build and clean commands.

- `make all` or `make` - Compiles the executable `schedtest`

- `make clean` - Removes the executable

## Run Instructions

### Manually
After compiling, use this command to run the executable with the chosen algorithm and input file:

    ./schedtest -alg [FIFO | SJF | PR] -input [FILE]

### Makefile
The Makefile also has several commands for running the different algorithms separately, together, and with Valgrind. All of the run commands will compile the code before running, so doing `make all` beforehand is unnecessary when using them. All of these commands assume the existence of the input file `input.txt`. Input files with other names will be ignored, so if other input files are desired it will need to be run manually.

- `make fifo` - Runs the executable with the FIFO, or FCFS, algorithm

- `make sjf` - Runs the executable with the SJF algorithm

- `make pr` - Runs the executable with the PR algorithm

- `make run` - Runs the three algorithms in order

- `make valfifo` - Runs the executable with the FIFO algorithm through Valgrind

- `make valsjf` - Runs the executable with the SJF algorithm through Valgrind

- `make valpr` - Runs the executable with the PR algorithm through Valgrind

- `make val` - Runs the three algorithms in order through Valgrind