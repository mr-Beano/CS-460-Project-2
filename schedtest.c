// CS 460 Operating Systems Project 2 - CPU Scheduling
// Dean Feller 3/3/2024

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define USAGE "Usage: ./schedtest -alg [FIFO|SJF|PR] -input [FILE]\n"

// For some reason my VS Code kept underlining CLOCK_MONOTONIC, 
// saying it was undefined even though a CTRL+click could find the definition.
// This is just here to get rid of the annoying red squiggly line ¯\_(ツ)_/¯.
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif



/* Structure to represent a process in this simulation. */
struct Process {
    int pid, priority;
    int numCPUBursts, numIOBursts;      // Amount of CPU/IO bursts
    int *cpuBursts, *ioBursts;          // Dynamic arrays to store CPU/IO burst times
    int cpuIndex, ioIndex;              // Indices into the CPU/IO burst arrays

    struct timespec begin;              // The time this process was created
    struct timespec turnaround;         // The total time this process took to complete

    struct timespec beginWait;          // The time this process was last added to the Ready queue
    struct timespec wait;               // The total time this process has spent in the Ready queue

    struct Process *prev, *next;
};

/*
Allocate a new Process, fill in its info, and return a pointer to it.
When you're done with a Process, be sure to free it with freeProcess() (not just free()!)
*/
struct Process* newProcess(int pid, int priority, int numCPUBursts, int numIOBursts, int cpuBursts[], int ioBursts[]) {
    struct Process *proc = (struct Process*) malloc(sizeof(struct Process));
    proc->pid = pid;
    proc->priority = priority;
    proc->numCPUBursts = numCPUBursts;
    proc->numIOBursts = numIOBursts;
    proc->cpuBursts = cpuBursts;
    proc->ioBursts = ioBursts;
    proc->cpuIndex = 0;
    proc->ioIndex = 0;
    clock_gettime(CLOCK_MONOTONIC, &proc->begin);
    proc->wait.tv_sec = 0;
    proc->wait.tv_nsec = 0;
    proc->prev = NULL;
    proc->next = NULL;
    return proc;
}

/* Free the given Process and all of its contents. */
void freeProcess(struct Process *proc) {
    if (proc != NULL) {
        free(proc->cpuBursts);
        free(proc->ioBursts);
        free(proc);
    }
}

/* Returns the total amount of CPU time remaining for the given process. */
int timeLeft(struct Process *proc) {
    int time = 0;
    for (int i = proc->cpuIndex; i < proc->numCPUBursts; i++) {
        time += proc->cpuBursts[i];
    }
    return time;
}



/* Implementation of a queue of Processes as a doubly-linked list. */
struct Queue {
    struct Process *head, *tail;
} *READYQ, *IOQ, *DONEQ;

/*
Allocate a new Queue, initialize its values, and return a pointer to it.
When you're done with a Queue, be sure to free it with freeQueue() (not just free()!)
*/
struct Queue* newQueue() {
    struct Queue *queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

/* Free the given queue and all of its contents. */
void freeQueue(struct Queue *queue) {
    struct Process *current = queue->head;
    struct Process *next;

    // While there are still Processes in the queue, save the next one,
    // free this one, then move over to the next one
    while (current != NULL) {
        next = current->next;
        freeProcess(current);
        current = next;
    }

    free(queue);
}

/* Returns 1 (true) if the given queue is empty, 0 if not. */
int isEmpty(struct Queue *queue) {
    if (queue->head == NULL && queue->tail == NULL) return 1;
    else return 0;
}

/* Print information about the given queue. */
void printQueue(struct Queue *queue, char* name, int verbose) {
    struct Process *current = queue->head;

    if (verbose) {
        printf("%s head is %p, tail is %p: ", name, queue->head, queue->tail);
        while (current != NULL) {
            printf("[");
            if (current->prev != NULL) printf("%d", current->prev->pid);
            else printf("%p", current->prev);
            printf(" <- %d -> ", current->pid);
            if (current->next != NULL) printf("%d", current->next->pid);
            else printf("%p", current->next);
            printf("] ");
            current = current->next;
        }
        printf("\n");
    }
    else {
        printf("[");
        while (current != NULL) {
            printf(" %d ", current->pid);
            current = current->next;
        }
        printf("]\n");
    }
}

/* Append proc to the end of the given queue. */
void addLast(struct Queue *queue, struct Process *proc) {
    if (proc != NULL) {
        if (isEmpty(queue)) {
            queue->head = proc;
            queue->tail = proc;
        }
        else {
            queue->tail->next = proc;
            proc->prev = queue->tail;
            queue->tail = proc;
        }
        if (queue == READYQ) clock_gettime(CLOCK_MONOTONIC, &proc->beginWait);
    }
}

/*
Removes the given process from the queue by reassigning the next/prev
pointers of its neighbors. In doing so, the next/prev pointers of proc
are set to NULL. Does nothing if proc is NULL.
*/
void removeProcess(struct Queue *queue, struct Process *proc) {
    if (proc != NULL) {
        if (proc == queue->head) queue->head = proc->next;
        if (proc == queue->tail) queue->tail = proc->prev;
        if (proc->prev != NULL) proc->prev->next = proc->next;
        if (proc->next != NULL) proc->next->prev = proc->prev;

        proc->prev = NULL;
        proc->next = NULL;

        // If we're removing from the Ready queue, increment the accumulated wait time
        // by however long was spent in the Ready queue
        if (queue == READYQ) {
            struct timespec endWait;
            clock_gettime(CLOCK_MONOTONIC, &endWait);
            proc->wait.tv_sec += endWait.tv_sec - proc->beginWait.tv_sec;
            proc->wait.tv_nsec += endWait.tv_nsec - proc->beginWait.tv_nsec;
        }
    }
}

/* Removes the first Process from the queue and returns a pointer to it. */
struct Process* removeFirst(struct Queue *queue) {
    struct Process *head = queue->head;
    removeProcess(queue, head);
    return head;
}

/*
Removes the Process with the highest priority from the queue
and returns a pointer to it.
*/
struct Process* removePriority(struct Queue *queue) {
    struct Process *current = queue->head;
    struct Process *highestPriority = current;

    // Iterate through the queue to find the Process with highest priority
    while (current != NULL) {
        if (current->priority > highestPriority->priority) highestPriority = current;
        current = current->next;
    }
    removeProcess(queue, highestPriority);
    return highestPriority;
}

/*
Removes the process with the shortest remaining CPU time from the queue
and returns a pointer to it.
*/
struct Process* removeShortest(struct Queue *queue) {
    struct Process *current = queue->head;
    struct Process *shortest = current;

    // Iterate through the queue to find the Process with the least time left
    while (current != NULL) {
        if (current->cpuBursts[current->cpuIndex] < shortest->cpuBursts[shortest->cpuIndex]) shortest = current;
        current = current->next;
    }
    removeProcess(queue, shortest);
    return shortest;
}




int cpuBusy = 0, ioBusy = 0, fileReadDone = 0;
sem_t cpuSem, ioSem;

enum Algorithm {
    FIFO,
    SJF,
    PR
};
const char* algStr[] = {"FIFO", "SJF", "PR"};

/*
Run by the readThread, opens the given input file and parses its lines.
The input lines can begin with either `proc`, `sleep`, or `stop`.
- `proc` is followed by several integers. The first is the priority, the second is 
the amount of bursts, and the rest are the alternating CPU/IO burst times.
- `sleep` is followed by an integer for the amount of time readThread should sleep in ms.
- `stop` denotes the end of the instructions, and tells the readThread to exit.
*/
void* fileRead(void* filename) {
    char* inputFileName = (char*) filename;

    FILE* inputFile = fopen(inputFileName, "r");
    if (inputFile == NULL) {
        printf("Failed to open file %s\n", inputFileName);
        exit(EXIT_FAILURE);
    }

    char* line = NULL;
    size_t len = 0;
    int currPid = 0;

    while(getline(&line, &len, inputFile) != -1) {
        char* token = strtok(line, " \n");

        // If line starts with 'proc', create a new Process, fill out its values,
        // and add it to the Ready queue.
        if (strcmp(token, "proc") == 0) {
            int pid = currPid++;
            int priority = atoi(strtok(NULL, " \n"));   // Get the first number
            int numBursts = atoi(strtok(NULL, " \n"));  // Get the second number

            // Since each process always begins and ends with a CPU burst, 
            // there will always be one more CPU burst than IO burst.
            int numIOBursts = (numBursts - 1) / 2;
            int numCPUBursts = numIOBursts + 1;

            int *cpuBursts = (int*) malloc(numCPUBursts * sizeof(int));
            int *ioBursts = (int*) malloc(numIOBursts * sizeof(int));

            // Fill out the arrays of CPU and IO burst times
            int cpuIndex = 0, ioIndex = 0;
            for (int i = 0; i < numBursts; i++) {
                int burst = atoi(strtok(NULL, " \n"));
                if (i % 2 == 0) {
                    cpuBursts[cpuIndex] = burst;
                    cpuIndex++;
                }
                else {
                    ioBursts[ioIndex] = burst;
                    ioIndex++;
                }
            }

            // Create a new Process with these values and add it to the Ready queue
            struct Process *proc = newProcess(pid, priority, numCPUBursts, numIOBursts, cpuBursts, ioBursts);
            addLast(READYQ, proc);

            sem_post(&cpuSem);
        }
        else if (strcmp(token, "sleep") == 0) {
            int sleepTime = atoi(strtok(NULL, " \n"));  // Get the first/only number
            usleep(sleepTime * 1000);
        }
        else if (strcmp(token, "stop") == 0) break;
    }
    
    free(line);
    fclose(inputFile);
    fileReadDone = 1;
    return NULL;
}


/*
Run by the cpuThread, chooses a process from the Ready queue according 
to the given algorithm and runs it (sleeps) for however long the CPU burst is.
*/
void* cpuScheduler(void* alg) {
    enum Algorithm *algorithm = (enum Algorithm*) alg;
    struct timespec waitTimespec = {1, 0};  // 1 second to wait for the semaphore before retrying

    while(1) {
        if (isEmpty(READYQ) && !cpuBusy && isEmpty(IOQ) && !ioBusy && fileReadDone) break;

        int semResult = sem_timedwait(&cpuSem, &waitTimespec);
        if (semResult == -1 && errno == ETIMEDOUT) continue;

        cpuBusy = 1;
        
        struct Process *proc;
        switch (*algorithm) {
            case FIFO:
                proc = removeFirst(READYQ);
                break;
            
            case SJF:
                proc = removeShortest(READYQ);
                break;
            
            case PR:
                proc = removePriority(READYQ);
                break;
        }

        if (proc != NULL) {
            usleep(proc->cpuBursts[proc->cpuIndex] * 1000);
            ++proc->cpuIndex;

            // If this proc's cpuIndex exceeds the num of CPU bursts, it just
            // finished its last burst. Save how much time it took to finish
            // and add it to the queue of completed processes.
            if (proc->cpuIndex >= proc->numCPUBursts) {
                struct timespec end;
                clock_gettime(CLOCK_MONOTONIC, &end);
                proc->turnaround.tv_sec = end.tv_sec - proc->begin.tv_sec;
                proc->turnaround.tv_nsec = end.tv_nsec - proc->begin.tv_nsec;

                addLast(DONEQ, proc);
            }
            else {
                addLast(IOQ, proc);
            }
        }

        cpuBusy = 0;
        sem_post(&ioSem);
    }

    return NULL;
}


/*
Run by the ioThread, chooses the next process from the I/O queue and runs it
(sleeps) for however long the I/O burst is.
*/
void* ioSystem() {
    struct timespec waitTimespec = {1, 0};  // 1 second to wait for the semaphore before retrying

    while(1) {
        if (isEmpty(READYQ) && !cpuBusy && isEmpty(IOQ) && !ioBusy && fileReadDone) break;

        int semResult = sem_timedwait(&ioSem, &waitTimespec);
        if (semResult == -1 && errno == ETIMEDOUT) continue;

        ioBusy = 1;

        struct Process *proc = removeFirst(IOQ);
        if (proc != NULL) {

            if (proc->ioIndex < proc->numIOBursts) {
                usleep(proc->ioBursts[proc->ioIndex] * 1000);
                proc->ioIndex++;
            }

            addLast(READYQ, proc);
        }
        
        ioBusy = 0;
        sem_post(&cpuSem);
    }

    return NULL;
}


/*
Initialize semaphores, spin up the FileRead, CPU, and IO threads, 
and join them when they're finished. Returns the time to completion.
*/
struct timespec runThreads(char* inputFileName, enum Algorithm* alg) {
    pthread_t readThread, cpuThread, ioThread;
    struct timespec begin, end, runtime;

    sem_init(&cpuSem, 0, 0);
    sem_init(&ioSem, 0, 0);

    if ((errno = pthread_create(&readThread, NULL, fileRead, inputFileName)) != 0) {
        printf("Failed to create file reader thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }
    if ((errno = pthread_create(&cpuThread, NULL, cpuScheduler, alg)) != 0) {
        printf("Failed to create CPU scheduler thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }
    if ((errno = pthread_create(&ioThread, NULL, ioSystem, NULL)) != 0) {
        printf("Failed to create I/O system thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &begin);
    
    if ((errno = pthread_join(readThread, NULL)) != 0) {
        printf("Failed to join file reader thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }
    if ((errno = pthread_join(cpuThread, NULL)) != 0) {
        printf("Failed to join CPU scheduler thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }
    if ((errno = pthread_join(ioThread, NULL)) != 0) {
        printf("Failed to join I/O system thread (errno %d)\n", errno);
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    runtime.tv_sec = end.tv_sec - begin.tv_sec;
    runtime.tv_nsec = end.tv_nsec - begin.tv_nsec;

    return runtime;
}

/* Convert a timespec's seconds and nanoseconds to milliseconds. */
double timespecToMS(struct timespec ts) {
    return (ts.tv_sec + (ts.tv_nsec / 1000000000.0)) * 1000;
}

/*
Print the input file name, scheduling algorithm used, throughput,
average turnaround time, and average waiting time in ready queue.
*/
void printOutput(char* inputFileName, enum Algorithm alg, struct timespec runtime) {
    int numProcs = 0;
    double totalTurnaroundTime = 0;
    double totalWaitTime = 0;

    struct Process *current = DONEQ->head;
    while (current != NULL) {
        numProcs++;
        totalTurnaroundTime += timespecToMS(current->turnaround);
        totalWaitTime += timespecToMS(current->wait);
        current = current->next;
    }
    
    printf("Input File Name               : %s\n", inputFileName);
    printf("CPU Scheduling Alg            : %s\n", algStr[alg]);
    printf("Throughput                    : %f proc/ms\n", numProcs / timespecToMS(runtime));
    printf("Avg. Turnaround Time          : %f ms\n", totalTurnaroundTime / numProcs);
    printf("Avg. Wait Time in Ready Queue : %f ms\n", totalWaitTime / numProcs);
}

int main(int argc, char* argv[]) {
    enum Algorithm alg = -1;
    char* inputFileName = NULL;
    
    READYQ = newQueue();
    IOQ = newQueue();
    DONEQ = newQueue();

    // Parse the arguments
    for (int i = 1; i < argc; i++) {

        // If we've reached the last argument, don't bother trying to get the alg or 
        // input file since they'll either have been gotten already, or are missing.
        if (i + 1 >= argc) break;

        // Get the algorithm to use
        if (strcmp(argv[i], "-alg") == 0) {
            if (strcmp(argv[i + 1], "FIFO") == 0) alg = FIFO;
            else if (strcmp(argv[i + 1], "SJF") == 0) alg = SJF;
            else if (strcmp(argv[i + 1], "PR") == 0) alg = PR;
        }
        // Get the input file
        else if (strcmp(argv[i], "-input") == 0) {
            inputFileName = argv[i + 1];
        }
    }

    // If the algorithm or filename haven't been set, exit with error
    if (alg == -1 || inputFileName == NULL) {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    struct timespec runtime = runThreads(inputFileName, &alg);

    printOutput(inputFileName, alg, runtime);

    freeQueue(READYQ);
    freeQueue(IOQ);
    freeQueue(DONEQ);
    return 0;
}