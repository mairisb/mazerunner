#ifndef MOVE_QUEUE_H
#define MOVE_QUEUE_H

#include <stdio.h>
#include <stdlib.h>

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
void addMove(int data);
int initMoveQueue(int numberOfNodes);
void freeMoveQueue();

#endif /* MOVE_QUEUE_H */
