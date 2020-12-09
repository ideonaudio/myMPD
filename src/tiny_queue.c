/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#include "../dist/src/sds/sds.h"
#include "log.h"
#include "tiny_queue.h"

tiny_queue_t *tiny_queue_create(void) {
    struct tiny_queue_t* queue = (struct tiny_queue_t *)malloc(sizeof(struct tiny_queue_t));
    assert(queue);
    queue->head = NULL;
    queue->tail = NULL;
    queue->length = 0;

    queue->mutex  = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    queue->wakeup = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    return queue;
}

void tiny_queue_free(tiny_queue_t *queue) {
    struct tiny_msg_t *current = queue->head;
    struct tiny_msg_t *tmp = NULL;
    while (current != NULL) {
        free(current->data);
        tmp = current;
        current = current->next;
        free(tmp);
    }
    free(queue);
}


int tiny_queue_push(tiny_queue_t *queue, void *data, long id) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_lock: %d", rc);
        return 0;
    }
    struct tiny_msg_t* new_node = (struct tiny_msg_t*)malloc(sizeof(struct tiny_msg_t));
    assert(new_node);
    new_node->data = data;
    new_node->id = id;
    new_node->timestamp = time(NULL);
    new_node->next = NULL;
    queue->length++;
    if (queue->head == NULL && queue->tail == NULL){
        queue->head = queue->tail = new_node;
    }
    else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    rc = pthread_mutex_unlock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
        return 0;
    }
    rc = pthread_cond_signal(&queue->wakeup);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_cond_signal: %d", rc);
        return 0;
    }
    return 1;
}

unsigned tiny_queue_length(tiny_queue_t *queue, int timeout) {
    timeout = timeout * 1000;  
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_lock: %d", rc);
        return 0;
    }
    if (timeout > 0 && queue->length == 0) {
        struct timespec max_wait = {0, 0};
        clock_gettime(CLOCK_REALTIME, &max_wait);
        //timeout in ms
        if (max_wait.tv_nsec <= (999999999 - timeout)) {
            max_wait.tv_nsec += timeout;
        } 
        else {
            max_wait.tv_sec += 1;
            max_wait.tv_nsec = timeout - (999999999 - max_wait.tv_nsec);
        }
        rc = pthread_cond_timedwait(&queue->wakeup, &queue->mutex, &max_wait);
        if (rc != 0 && rc != ETIMEDOUT) {
            LOG_ERROR("Error in pthread_cond_timedwait: %d", rc);
        }
    }
    unsigned len = queue->length;
    rc = pthread_mutex_unlock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
    }
    return len;
}

void *tiny_queue_shift(tiny_queue_t *queue, int timeout, long id) {
    timeout = timeout * 1000;
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_lock: %d", rc);
        return 0;
    }
    if (queue->length == 0) {
        if (timeout > 0) {
            struct timespec max_wait = {0, 0};
            clock_gettime(CLOCK_REALTIME, &max_wait);
            //timeout in ms
            if (max_wait.tv_nsec <= (999999999 - timeout)) {
                max_wait.tv_nsec += timeout;
            }
            else {
                max_wait.tv_sec += 1;
                max_wait.tv_nsec = timeout - (999999999 - max_wait.tv_nsec);
            }
            rc = pthread_cond_timedwait(&queue->wakeup, &queue->mutex, &max_wait);
            if (rc != 0) {
                if (rc != ETIMEDOUT) {
                    LOG_ERROR("Error in pthread_cond_timedwait: %d", rc);
                    LOG_ERROR("nsec: %ld", max_wait.tv_nsec);
                }
                rc = pthread_mutex_unlock(&queue->mutex);
                if (rc != 0) {
                    LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
                }
                return NULL;
            }
        }
        else {
            rc = pthread_cond_wait(&queue->wakeup, &queue->mutex);
            if (rc != 0) {
                LOG_ERROR("Error in pthread_cond_wait: %d", rc);
                rc = pthread_mutex_unlock(&queue->mutex);
                if (rc != 0) {
                    LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
                }
                return NULL;
            }
        }
    }
    //queue has entry
    if (queue->head != NULL) {
        struct tiny_msg_t *current = NULL;
        struct tiny_msg_t *previous = NULL;
        
        for (current = queue->head; current != NULL; previous = current, current = current->next) {
            if (id == 0 || id == current->id) {
                void *data = current->data;
                
                if (previous == NULL) {
                    //Fix beginning pointer
                    queue->head = current->next;
                }
                else {
                    //Fix previous nodes next to skip over the removed node.
                    previous->next = current->next;
                }
                //Fix tail
                if (queue->tail == current) {
                    queue->tail = previous;
                }

                free(current);
                queue->length--;
                rc = pthread_mutex_unlock(&queue->mutex);
                if (rc != 0) {
                    LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
                }
                return data;
            }
            LOG_DEBUG("Skipping queue entry with id %d", current->id);
        }
    }

    rc = pthread_mutex_unlock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
    }
    return NULL;
}


void *tiny_queue_expire(tiny_queue_t *queue, time_t max_age) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_lock: %d", rc);
        return 0;
    }
    //queue has entry
    if (queue->head != NULL) {
        struct tiny_msg_t *current = NULL;
        struct tiny_msg_t *previous = NULL;
        
        time_t expire_time = time(NULL) - max_age;
        
        for (current = queue->head; current != NULL; previous = current, current = current->next) {
            if (max_age == 0 || current->timestamp < expire_time) {
                void *data = current->data;
                
                if (previous == NULL) {
                    //Fix beginning pointer
                    queue->head = current->next;
                }
                else {
                    //Fix previous nodes next to skip over the removed node.
                    previous->next = current->next;
                }
                //Fix tail
                if (queue->tail == current) {
                    queue->tail = previous;
                }

                free(current);
                queue->length--;
                rc = pthread_mutex_unlock(&queue->mutex);
                if (rc != 0) {
                    LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
                }
                LOG_WARN("Found expired entry in queue");
                return data;
            }
        }
    }

    rc = pthread_mutex_unlock(&queue->mutex);
    if (rc != 0) {
        LOG_ERROR("Error in pthread_mutex_unlock: %d", rc);
    }
    return NULL;
}
