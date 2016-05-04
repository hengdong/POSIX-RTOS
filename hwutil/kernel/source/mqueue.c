/*
 * File         : mqueue.c
 * This file is part of POSIX-RTOS
 * COPYRIGHT (C) 2015 - 2016, DongHeng
 * 
 * Change Logs:
 * DATA             Author          Note
 * 2015-12-11       DongHeng        create
 */

#include "mqueue.h"
#include "pthread.h"
#include "sched.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "semaphore.h"

/* class of the message queue */
struct mq
{
    char                *mq_pbuf;
    size_t              mq_pbuf_size;
    
    char                *send_ptr;
    char                *recv_ptr;

    struct mq_attr      mq_attr;
    
    sem_t               sem;
};
typedef struct mq mq_t;

/* class of the message of the message queue */
struct mq_msg
{
    size_t              msg_size;
    int                 used;            
};
typedef struct mq_msg mq_msg_t;

/******************************************************************************/

/*
 * mq_open - open a message queue
 *
 * @param name    the name of the messagequeue
 * @param flag    the flag of the openning attribute of the message queue
 * @param mode    the mode of the openning attribute of the message queue
 * @param mq_attr the attribute of the message queue 
 *
 * @return the result
 */
mqd_t mq_open (const char *name, int flag, ...)
{
    va_list args;
    mode_t *mode;
    struct mq_attr *mq_attr;
    mq_t *mq;
    char *buffer;
  
    /* check name */
    if (!name)
        return -1;
    
    /* chack flag */
/*
    if (!(flag & O_CREAT))
        return -1;
*/
    
    /* check mode */
    va_start(args, flag);   
    mode = (mode_t *)args; 
    if (*mode != flag)
        return -1;
  
    /* check attribute */
    va_arg(args, struct mq_attr *);
    mq_attr = (struct mq_attr *)(*(int *)args);
    if ((mq_attr->mq_maxmsg * mq_attr->mq_msgsize) > (MQ_MSG_SIZE_MAX * MQ_MSG_NUM_MAX))
        return -1;
    
    /* malloc message buffer */
    if (!(buffer = calloc((sizeof(mq_msg_t) + mq_attr->mq_msgsize) * mq_attr->mq_maxmsg)))
        return -1;    
    
    /* malloc message queue */
    if (!(mq = calloc(sizeof(mq_t))))
        return -1;
     
    /* init the message queue */
    mq->mq_pbuf = buffer;
    memcpy(&mq->mq_attr, mq_attr, sizeof(struct mq_attr));
    mq->send_ptr = mq->recv_ptr = mq->mq_pbuf;
    mq->mq_attr.mq_flags = flag;
    mq->mq_attr.mq_curmsgs = 0;
    mq->mq_pbuf_size = (sizeof(mq_msg_t) + mq_attr->mq_msgsize) * mq_attr->mq_maxmsg;
    
    if (!(mq->mq_attr.mq_flags & O_NONBLOCK))
        if (sem_init(&mq->sem, 0, mq->mq_attr.mq_maxmsg))
            return -1;

    /* return the message queue address */
    return (mqd_t)mq;
}

/*
 * mq_send - send a message to the message queue
 *
 * @param mqdes     the handle oft he message queue
 * @param msg_ptr   send message point
 * @param msg_len   send message length
 * @param msg_prio  send message priority
 *
 * @return the result
 */
ssize_t mq_send (mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    mq_t *mq = (mq_t *)mqdes;
    mq_msg_t *mq_msg;
    char *send_ptr;
    ssize_t ret;
    
    /* check current message numbers at the message queue */
    if (mq->mq_attr.mq_curmsgs >= mq->mq_attr.mq_maxmsg)
        return -1;
    
    /* check the size of current message to be sent */
    if (msg_len > mq->mq_attr.mq_msgsize)
        return -1;
    
    /* get send message point */
    mq_msg = (mq_msg_t *)mq->send_ptr;
    /* check if send buffer is free */
    if (!mq_msg->used)
    {   
        /* fill the data */
        mq_msg->msg_size = msg_len;
        send_ptr = mq->send_ptr + sizeof(mq_msg_t);
        memcpy(send_ptr, msg_ptr, msg_len);
        
        /* point to next buffer */
        mq->send_ptr = mq->send_ptr + mq->mq_attr.mq_msgsize + sizeof(mq_msg_t);
        if (mq->send_ptr >= mq->mq_pbuf + mq->mq_pbuf_size)
            mq->send_ptr = mq->mq_pbuf;
        /* add the current message number */
        mq->mq_attr.mq_curmsgs++;
        
        ret = msg_len;
       
        /* make the buffer is send */
        mq_msg->used = 1;
    }
    else
        ret = -1;
    
    /* post a sem to the recv thread */
    if (!(mq->mq_attr.mq_flags & O_NONBLOCK))
        sem_post(&mq->sem);
  
    return ret;
}

/*
 * mq_receive - recieve a message from themessage queue
 *
 * @param mqdes     the handle oft he message queue
 * @param msg_ptr   recieve message point
 * @param msg_len   recieve message length
 * @param msg_prio  recieve message priority
 *
 * @return the result
 */
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    mq_t *mq = (mq_t *)mqdes;
    mq_msg_t *mq_msg;
    char *recv_ptr;
    ssize_t ret;
  
    /* wait until recv buffer is not empty */
    if (!(mq->mq_attr.mq_flags & O_NONBLOCK))
        sem_wait(&mq->sem);
    
    /* check current message numbers at the message queue */
    if (!mq->mq_attr.mq_curmsgs)
        return -1;
    
    /* get recv message point */
    mq_msg = (mq_msg_t *)mq->recv_ptr;
    /* check if send buffer is free */
    if (mq_msg->used)
    {   
        /* fill the data */
        recv_ptr = mq->recv_ptr + sizeof(mq_msg_t);
        memcpy(msg_ptr, recv_ptr, mq_msg->msg_size);
        
        /* point to next buffer */
        mq->recv_ptr = mq->recv_ptr + mq->mq_attr.mq_msgsize + sizeof(mq_msg_t);
        if (mq->recv_ptr >= mq->mq_pbuf + mq->mq_pbuf_size)
            mq->recv_ptr = mq->mq_pbuf;
        /* add the current message number */
        mq->mq_attr.mq_curmsgs--;
        
        ret = mq_msg->msg_size;
        
        /* make the buffer is recved */
        mq_msg->used = 0;
    }
    else
        ret = -1;  
    
    return ret;
}

