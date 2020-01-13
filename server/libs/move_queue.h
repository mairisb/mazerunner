#ifndef MOVE_QUEUE_H
#define MOVE_QUEUE_H

#include <stdio.h>
#include <stdlib.h>

/******************************************************************
 The move queue is a special linked list data structure type that
 allocates the given amount of nodes on initialization and it's
 current size is controlled by the head pointer.
 Useful for storing and removing queues because adding/removing
 elements doesn't require malloc which could slow things down.
 Also useful for implementing the last player move resolution mode
 because only a few links need to be rearranged.
******************************************************************/

struct Node {
    struct Node *next;
    int data;
};

struct QueueList {
    struct Node *start;
    struct Node *end;
    struct Node *head;
};

struct QueueList *moveQueue;

struct Node *createNode();
int createQueueList();
int appendNewNode();
void replaceMove(int data);
void addMove(int data);
int initMoveQueue(int numberOfNodes);
void freeMoveQueue();

#endif /* MOVE_QUEUE_H */
