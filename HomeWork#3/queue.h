//
//  Queue.h
//  os3
//
//  Created by Baraa Egbaria on 06/01/2023.
//

#ifndef Queue_h
#define Queue_h

typedef struct timeval Timeval ;

typedef struct node_t {
    Timeval time;
    int sfd;
    struct node_t *next;
} *Node;

typedef struct Queue_t
{
    Node header;
    Node last;
    int size;
    int max_size;
} *Queue;

Queue queueCreate(int max_size);
void queueDestroy(Queue queue);
int queueGetSize(Queue queue);
int queueGetMaxSize(Queue queue);

void queuePushBack(Queue queue, int sfd, Timeval time);
int queuePopFirst(Queue queue, Timeval* time);
int queuePopIndex(Queue queue, int index, Timeval* time);

#endif /* Queue_h */
