#include "move_queue.h"
#include <stdio.h>
#include <stdlib.h>

struct Node *createNode() {
    struct Node *temp = malloc(sizeof(struct Node));
    if(temp == NULL) {
        fprintf(stderr, "Memory allocation of QueueList Node failed\n");
        return NULL;
    }

    temp->next = NULL;

    return temp;
}

int createQueueList() {
    moveQueue = malloc(sizeof(struct QueueList));
    if(moveQueue == NULL) {
        fprintf(stderr, "Memory allocation of QueueList failed\n");
        return -1;
    }

    moveQueue->start = NULL;
    moveQueue->end = NULL;
    moveQueue->head = NULL;

    return 0;
}

int appendNewNode() {
    struct Node *node = createNode();
    if (node == NULL) {
        return -1;
    }

    if (moveQueue->start == NULL) {
        moveQueue->start = node;
        moveQueue->end = node;
    } else {
        moveQueue->end->next = node;
        moveQueue->end = node;
    }

    return 0;
}

void replaceMove(int data) {
    struct Node *targetNode;

    if (moveQueue->head == NULL) {
        moveQueue->head = moveQueue->start;
        moveQueue->head->data = data;
    } else if (moveQueue->start->data == data) {
        if (moveQueue->head != moveQueue->start) {
            targetNode = moveQueue->start;
            moveQueue->start = moveQueue->start->next;
            targetNode->next = moveQueue->head->next;
            moveQueue->head->next = targetNode;
            if (moveQueue->head == moveQueue->end) {
                moveQueue->end = targetNode;
            }
            moveQueue->head = targetNode;
        }
    } else {
        struct Node *temp = moveQueue->start;
        targetNode = moveQueue->start->next;
        while (temp != moveQueue->head) {
            if (targetNode->data == data) {
                if (moveQueue->head != targetNode) {
                    temp->next = targetNode->next;
                    targetNode->next = moveQueue->head->next;
                    moveQueue->head->next = targetNode;
                    if (moveQueue->head == moveQueue->end) {
                        moveQueue->end = targetNode;
                    }
                    moveQueue->head = targetNode;
                }
                return;
            }

            targetNode = targetNode->next;
            temp = temp->next;
        }

        if (moveQueue->head != moveQueue->end) {
            moveQueue->head = moveQueue->head->next;
            moveQueue->head->data = data;
        } else {
            printf("QueueList is full, cannot satisfy addMove request\n");
        }
    }
}

void addMove(int data) {
    if (moveQueue->head == NULL) {
        moveQueue->head = moveQueue->start;
    } else if (moveQueue->head != moveQueue->end) {
        moveQueue->head = moveQueue->head->next;
    } else {
        printf("QueueList is full, cannot satisfy addMove request\n");
        return;
    }

    moveQueue->head->data = data;
}

int initMoveQueue(int numberOfNodes) {
    int i;
    if (createQueueList() < 0) {
        return -1;
    }

    for (i = 0; i < numberOfNodes; i++) {
        if (appendNewNode() < 0) {
            return -1;
        }
    }

    return 0;
}

void freeMoveQueue() {
    struct Node *node = moveQueue->start;
    while (node != NULL) {
        struct Node *temp = node->next;
        free(node);
        node = temp;
    }

    free(moveQueue);
}
