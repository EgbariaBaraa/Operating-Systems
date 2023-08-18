

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

#include "segel.h"
#include "request.h"
#include "stdbool.h"
#include "queue.h"
#include <math.h>

#define MAX_SCHED 7
#define BLOCK 1
#define BLOCK_FLUSH 5
#define DYNAMIC 6
#define DROP_HEAD 2
#define DROP_TAIL 3
#define RANDOM 4

int currently_waiting, curretly_running;
pthread_cond_t expecting_req, obtainable_req;
pthread_mutex_t lock;

// int dynamic_req_counter, static_req_counter, threads_counter;
Queue waiting_requests_queue;

void initPthreadsVariables(void)
{
    currently_waiting = curretly_running = 0;
    pthread_cond_init(&expecting_req, NULL);
    pthread_cond_init(&obtainable_req, NULL);
    pthread_mutex_init(&lock, NULL);
    // threads_counter = dynamic_req_counter = static_req_counter = 0;
    waiting_requests_queue = NULL;
}

// HW3: Parse the new arguments too
void getargs(int *port, int *threads_num, int *requests_limit, int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads_num = atoi(argv[2]);
    *requests_limit = atoi(argv[3]);
}

int threads_counter = 0;

void *action(void *arg)
{
    int thread_id = *(int *)arg;
    int thread_static_req_count, thread_dynamic_req_count, thread_req_count;
    thread_req_count = thread_static_req_count = thread_dynamic_req_count = 0;
    Timeval arrival_time, temp, end;

    while (1)
    {
        pthread_mutex_lock(&lock);
        while (queueGetSize(waiting_requests_queue) == 0)
            pthread_cond_wait(&obtainable_req, &lock);

        int sfd = queuePopFirst(waiting_requests_queue, &arrival_time);
        curretly_running++;
        currently_waiting--;
        pthread_mutex_unlock(&lock);

        gettimeofday(&temp, NULL);
        timersub(&temp, &arrival_time, &end);
        requestHandle(sfd, arrival_time, end, thread_id, &thread_req_count,
                      &thread_static_req_count, &thread_dynamic_req_count);
        Close(sfd);
        pthread_mutex_lock(&lock);
        curretly_running--;
        pthread_cond_signal(&expecting_req);
        pthread_mutex_unlock(&lock);
       
    }
    return NULL;
}

void BlockHandler(int requests_limit, int connfd, Timeval time)
{
    while (queueGetSize(waiting_requests_queue) + curretly_running == requests_limit)
        pthread_cond_wait(&expecting_req, &lock);
    queuePushBack(waiting_requests_queue, connfd, time);
    currently_waiting++;
    pthread_cond_signal(&obtainable_req);
}

void BlockFlushHandler(int requests_limit, int connfd, Timeval time)
{
    while (queueGetSize(waiting_requests_queue) + curretly_running > 0)
        pthread_cond_wait(&expecting_req, &lock);
    Close(connfd);
}

void DropTailHandler(int connfd)
{
    Close(connfd);
}

void DropHeadHandler(int connfd, Timeval time)
{
    if (queueGetSize(waiting_requests_queue) == 0)
    {
        Close(connfd);
        return;
    }
    Timeval time1;
    int fd = queuePopFirst(waiting_requests_queue, &time1);
    Close(fd);
    queuePushBack(waiting_requests_queue, connfd, time);
}

void DynamicHandler(int* requests_limit, int max_limit, int connfd, Timeval time)
{
    if(  max_limit > *requests_limit)
        *requests_limit += 1;
    Close(connfd);
}

void RandomHandler(int connfd, Timeval time)
{
    int size = queueGetSize(waiting_requests_queue);
    if (size == 0)
    {
        Close(connfd);
        return;
    }
    Timeval time1;
    size = ceil(size / 2);
    while ((size--) > 0)
    {
        int index = rand() % queueGetSize(waiting_requests_queue);
        int to_close=queuePopIndex(waiting_requests_queue, index, &time1);
        if (to_close == -1) break;
        Close(to_close);
        currently_waiting--;
    }
    queuePushBack(waiting_requests_queue, connfd, time);
    currently_waiting++;
    
}

int checkScheduler(char *scheduler)
{
    if (!strcmp(scheduler, "block"))
        return BLOCK; // BlockHandler(requests_limit, connfd, time);
    else if (!strcmp(scheduler, "bf"))
        return BLOCK_FLUSH; // BlockFlushHandler(requests_limit, connfd, time);
    else if (!strcmp(scheduler, "dt"))
        return DROP_TAIL; // DropTailHandler(connfd);
    else if (!strcmp(scheduler, "dh"))
        return DROP_HEAD; // DropHeadHandler(connfd, time);
    else if (!strcmp(scheduler, "random"))
        return RANDOM; // RandomHandler(connfd, time);
    else if (!strcmp(scheduler, "dynamic"))
        return DYNAMIC; // DynamicHandler(connfd, time);
    return -1;
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, max_limit = 0;
    struct sockaddr_in clientaddr;

    initPthreadsVariables();

    char scheduler[MAX_SCHED] = {0};
    int requests_limit = 0, threads_num = 0;

    getargs(&port, &threads_num, &requests_limit, argc, argv);
    strcpy(scheduler, argv[4]);
    if(!strcmp(scheduler,"dynamic"))
        max_limit = atoi(argv[5]);

    waiting_requests_queue = queueCreate(requests_limit);

    pthread_t *threads_array = malloc(sizeof(pthread_t) * threads_num);
    for (int i = 0; i < threads_num; i++)
    {
        int *index = malloc(sizeof(int));
        *index = i;
        pthread_create(threads_array + i, NULL, action, index);
    }

    // TODO: MAYBE COUNTERS ON HEAP ?

    listenfd = Open_listenfd(port);
    while (1)
    {

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        Timeval time;
        gettimeofday(&time, NULL);

        int sched = checkScheduler(scheduler);
        pthread_mutex_lock(&lock);
        if (queueGetSize(waiting_requests_queue) + curretly_running < requests_limit)
        {
            queuePushBack(waiting_requests_queue, connfd, time);
            currently_waiting++;
            pthread_cond_signal(&obtainable_req);
            pthread_mutex_unlock(&lock);
            continue;
        }
        else
        {
            if (sched == BLOCK)
                BlockHandler(requests_limit, connfd, time);
            else if (sched == BLOCK_FLUSH)
                BlockFlushHandler(requests_limit, connfd, time);
            else if (sched == DYNAMIC)
                DynamicHandler(&requests_limit, max_limit, connfd, time);
            else if (sched == DROP_TAIL)
                DropTailHandler(connfd);
            else if (sched == DROP_HEAD)
                DropHeadHandler(connfd, time);
            else if (sched == RANDOM)
                RandomHandler(connfd, time);
        }
        pthread_mutex_unlock(&lock);
    }
}
