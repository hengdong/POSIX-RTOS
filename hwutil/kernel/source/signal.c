/*
 * File         : signal.c
 * This file is part of POSIX-RTOS
 * COPYRIGHT (C) 2015 - 2016, DongHeng
 * 
 * Change Logs:
 * DATA             Author          Note
 * 2015-12-11       DongHeng        create
 */

#include "signal.h"
#include "sched.h"
#include "stdlib.h"

#ifndef SIG_EVENT_TABLE
    #define SIG_EVENT_TABLE     32
#else
    #if SIG_EVENT_TABLE < 8
        #error "SIG_EVENT_TABLE is too small"
    #endif
#endif
   
/*@{*/    
   
static struct sigevent_list sigevent_table[SIG_EVENT_TABLE];

/*@}*/

/*
 * __sigevent_alloc - alloc a free sigevent block
 *
 * return the free sigevent block point
 */
INLINE struct sigevent_list *__sigevent_alloc(void)
{
    int i;
    
    for (i = 0; i < SIG_EVENT_TABLE; i++)
    {
        if (!sigevent_table[i].sigevent.flag)
        {
            sigevent_table[i].sigevent.flag = 1;
            
            return &sigevent_table[i];
        }
    }
    
    return NULL;
}

/*
 * __sigevent_free - free sigevent block
 */
INLINE void __sigevent_free(struct sigevent_list *sigevent)
{
    sigevent->sigevent.flag = 0;
}

/*
 * __sigevent_handle - handle the thread signal queue data
 */
static void* __sigevent_handle(void *arg)
{
    os_pthread_t *pthread = (os_pthread_t *)arg;
    struct sigevent_list *sigevent;
     
    LIST_FOR_EACH_HEAD_NEXT(sigevent,
                            &pthread->sigevent_list,
                            struct sigevent_list,
                            list)
    {
        struct sigevent_list *sig;
      
        LIST_FOR_EACH_ENTRY(sig,
                            &pthread->sig_list,
                            struct sigevent_list,
                            list)
        {
            if (sig->sigevent.sigev_signo == sigevent->sigevent.sigev_signo)
            {
                sig->sigevent.sigev_notify_function(sigevent->sigevent.sigev_value);
            }
        }
        
        __sigevent_free(sigevent);
        
        list_remove_node(&sigevent->list);
    }
    
    return 0;
}

/*
 * __sigevent_report - report the signal to thread
 *
 * @param pthread the thread handle
 * @param sigevent the signal event point
 *
 */
INLINE void __sigevent_report(os_pthread_t *pthread, struct sigevent_list *sigevent)
{
    phys_reg_t temp = hw_interrupt_suspend();
  
    list_insert_tail(&pthread->sigevent_list, &sigevent->list);
    
    __sched_report_signal(pthread, __sigevent_handle, pthread);
    
    hw_interrupt_recover(temp);
}

/*
 * signal - register the signal and its generating function
 *
 * @param signum signal number
 * @param handler signal generating function
 *
 * return the result
 */
sighandler_t signal (int signum, sighandler_t handler)
{
    os_pthread_t *current_thread;
    struct sigevent_list *sigevent;
    phys_reg_t temp;
  
    current_thread = get_current_thread();
    
    if ((sigevent = malloc(sizeof(struct sigevent_list))) == NULL)
        return NULL;
    
    sigevent->sigevent.sigev_signo = signum;
    sigevent->sigevent.sigev_notify_function = handler;
    sigevent->sigevent.sigev_value.sival_int = 0;
    
    temp = hw_interrupt_suspend();
    
    list_insert_tail(&current_thread->sig_list, &sigevent->list);
    
    hw_interrupt_recover(temp);
    
    return handler;
}

int sigqueue (pid_t pid, int signo, const union sigval value)
{
    os_pthread_t *pthread;
    struct sigevent_list *sigevent;
    
    if ((sigevent = __sigevent_alloc()) == NULL)
        return EAGAIN;
      
    pthread = (os_pthread_t *)pid;
    
    sigevent->sigevent.sigev_signo = signo;
    sigevent->sigevent.sigev_value = value;
    
    __sigevent_report(pthread, sigevent);
    
    return 0;
}
