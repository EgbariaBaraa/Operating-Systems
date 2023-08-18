//
//  Queue.c
//  os3
//
//  Created by Baraa Egbaria on 06/01/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "queue.h"

static Node nodeCreate(Queue queue, int sfd, Timeval time)
{
    Node new_node = malloc(sizeof(*new_node));
    if(new_node == NULL)
        return NULL;
    new_node->sfd = sfd;
    new_node->time = time;
    new_node->next = NULL;
    return new_node;
}

static void nodeDestroy(Node node)
{
    free(node);
}

Queue queueCreate(int max_size)
{
    Queue queue = malloc(sizeof(*queue));
    if(queue == NULL)
    {
        return NULL;
    }
    queue->header = NULL;
    queue->last = queue->header;
    queue->size = 0;
    queue->max_size = max_size;
    return queue;
}

void queueDestroy(Queue queue)
{
    if(queue == NULL)
        return;
    free(queue);
}

int queueGetSize(Queue queue)
{
    if(queue == NULL)
        return -1;
    return queue->size;
}

int queueGetMaxSize(Queue queue)
{
    if(queue == NULL)
        return -1;
    return queue->max_size;
}

void queuePushBack(Queue queue, int sfd, Timeval time)
{
    if(queueGetSize(queue) == queue->max_size)
        return;
    
    Node node = nodeCreate(queue, sfd, time);
    if(queueGetSize(queue) == 0)
    {
        queue->header = node;
        queue->last = node;
    }
    else
    {
        queue->last->next = node;
        queue->last = queue->last->next;
    }
    queue->size += 1;
}

int queuePopFirst(Queue queue, Timeval* time)
{
    if(queue == NULL || queueGetSize(queue) == 0)
    {
        time = NULL;
        return -1;
    }
    Node to_pop = queue->header;
    int sfd = to_pop->sfd;
    *time = to_pop->time;
    
    if(queueGetSize(queue) == 1)
        queue->header = queue->last = NULL;
    else if(queueGetSize(queue) == 2)
        queue->header = queue->last;
    else
        queue->header = queue->header->next;
    
    nodeDestroy(to_pop);
    queue->size--;
    return sfd;
}

int queuePopIndex(Queue queue, int index, Timeval* time)
{
    if(queue == NULL || queueGetSize(queue) <= index || index < 0)
        return -1;
    else if(index > 0)
    {
        Node curr = queue->header, prev = NULL;
        while(0<index)
        {
            prev = curr;
            curr = curr->next;
            index--;
        }
        int sfd = curr->sfd;
        *time = curr->time;
        prev->next = curr->next;
        if(curr->next == NULL)
            queue->last = prev;
        nodeDestroy(curr);
        queue->size--;
        return sfd;
    }
    else
        return queuePopFirst(queue, time);
}
