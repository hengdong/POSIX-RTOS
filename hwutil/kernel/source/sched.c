/*
 * File         : shed.c
 * This file is part of POSIX-RTOS
 * COPYRIGHT (C) 2015 - 2016, DongHeng
 * 
 * Change Logs:
 * DATA             Author          Note
 * 2015-11-23       DongHeng        create
 */

#include "sched.h"
#include "list.h"
#include "string.h"
#include "debug.h"
#include "stdio.h"

/*@{*/

#if 0
#define SCHEDULER_DEBUG( x )     printk x
#else
#define SCHEDULER_DEBUG( x )
#endif

#define SCHEDULER_IS_LOCKED      1
#define SCHEDULER_IS_UNLOCKED    0

/******************************************************************************/

#define SCHED_LOCK_PROC() \
            if (SCHEDULER_IS_LOCKED == sched.locked) \
                return;

/******************************************************************************/

/* CPU usage structure dscription */
struct usage
{
    /* ticks for a minute */
    os_u16              min_ticks;
    os_u16              cur_ticks;
    
    /* ticks for system is not idle */
    os_u16              run_count;
    
    os_u16              usage;
};

/* sheduler structure dscription */
struct sched
{
    os_u32              thread_ready_group;
    list_t              thread_ready_table[PTHREAD_PRIORITY_MAX + 1];
    
    list_t              thread_sleep_list;
    list_t              thread_delete_list;
    
    os_pthread_t        *current_thread;
    list_t              thread_list;
    
    struct usage        usage;
    
    os_u32              locked;
};

/*@}*/

/*@{*/

/* system scheduler structure */
static struct sched sched KERNEL_SECTION;

NO_INIT os_u32 thread_switch_interrupt_flag KERNEL_SECTION;
NO_INIT os_u32 interrupt_from_thread KERNEL_SECTION;
NO_INIT os_u32 interrupt_to_thread KERNEL_SECTION;
/*@}*/

/*@{*/

/**
  * sched_get_highest_ready_group - This function will get the highest priority 
  *                                 thread
  *
  * @param read_group the read threads record bit goupe
  *
  * @return the highest priority thread number
  */
INLINE os_u32 sched_get_highest_ready_group(os_u32 read_group)
{
    os_u32 offset = 0;
    /* sheduler thread priority remap table */
    STATIC OS_RO os_u8 scheduler_priority_remap_table[OS_U8_MAX + 1] = 
    {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    };
  
    if (read_group & 0xff000000)
    {
        read_group >>= 24;
        offset = 24;
    }
    else if (read_group & 0x00ff0000)
    {
        read_group >>= 16;
        offset = 16;
    }
    else if (read_group & 0x0000ff00)
    {
        read_group >>= 8;
        offset = 8;
    }
    
    return scheduler_priority_remap_table[read_group] + offset;
}

/**
  * sched_start - This function will start the operation system scheduler
  */
void sched_start(void)
{
    extern void hw_context_switch_to(phys_reg_t sp);
  
    os_u32 thread_ready_group_num = sched_get_highest_ready_group(sched.thread_ready_group);
    
    scheduler.current_thread = LIST_HEAD_ENTRY(&sched.thread_ready_table[thread_ready_group_num],
                                               os_pthread_t,
                                               list);
    
    hw_context_switch_to((phys_reg_t)&sched.current_thread->sp);
}

/**
  * sched_run - This function will start the operation system sheduler
  */
void sched_run(void)
{
    os_u32 thread_ready_group_num;
    os_pthread_t *to_thread;
    phys_reg_t temp;  
    extern void hw_context_switch(phys_reg_t from_thread, phys_reg_t to_thread);
    
    /* suspend the hardware interrupt for the preparing for context switching */
    temp = hw_interrupt_suspend();
      
    thread_ready_group_num = sched_get_highest_ready_group(sched.thread_ready_group);
    
    to_thread = LIST_HEAD_ENTRY(&sched.thread_ready_table[thread_ready_group_num],
                                os_pthread_t,
                                list);
    
    /* we do context switching if current highest priority thread being changed */
    if (to_thread != sched.current_thread)
    {
        os_pthread_t *from_thread;
        extern void scheduler_active_privilege(void);
        
        from_thread = sched.current_thread;
        sched.current_thread = to_thread;
        
        SCHEDULER_DEBUG(("frome [0x%08x] to [0x%08x]\r\n", from_thread, to_thread));
        
        if (to_thread->status != PTHREAD_STATE_INT && from_thread->status != PTHREAD_STATE_INT)
            hw_context_switch((phys_reg_t)&from_thread->sp, (phys_reg_t)&to_thread->sp);
        else if (to_thread->status == PTHREAD_STATE_INT && from_thread->status != PTHREAD_STATE_INT)
            hw_context_switch((phys_reg_t)&from_thread->sp, (phys_reg_t)&to_thread->int_sp);
        else if (to_thread->status != PTHREAD_STATE_INT && from_thread->status == PTHREAD_STATE_INT)
            hw_context_switch((phys_reg_t)&from_thread->int_sp, (phys_reg_t)&to_thread->sp);
        else if (to_thread->status == PTHREAD_STATE_INT && from_thread->status == PTHREAD_STATE_INT)
            hw_context_switch((phys_reg_t)&from_thread->int_sp, (phys_reg_t)&to_thread->int_sp);
        
        SCHEDULER_DEBUG(("thread switched.\r\n"));
    }
    
    /* recover the system or usr interrupt status */
    hw_interrupt_recover(temp);
}

/**
 * This function will wake up the sleeping thread if it is timeout
 */
INLINE void sched_wakeup_thread(void)
{
    os_pthread_t *thread, *old_thread; 
    
    LIST_FOR_EACH_ENTRY(old_thread, 
                        &sched.thread_sleep_list,
                        os_pthread_t,
                        list)
    {       
        /* active the suspend thread and put it on ready group */
        old_thread->sleep_ticks--;
            
        /* check the sleeping time */
        if (!old_thread->sleep_ticks)
        {
            thread = old_thread;
          
            /* get the prev thread */ 
            old_thread = LIST_TAIL_ENTRY(&thread->list, os_pthread_t, list);
            
            /* wakeup the thread */
            sched_set_thread_ready(thread);
        }
    }  
}

/**
  * This function will wake up the sleeping thread if it is timeout
  */
INLINE void sched_cpu_usage(void)
{
    os_pthread_t *thread = PTHREAD_POINT(get_current_thread()); 

    sched.usage.cur_ticks++;
    
    /* count the ticks when the cpu is running */
    if (PTHREAD_TYPE_IDLE != thread->type)
    {
    	sched.usage.run_count += (1000 / RTOS_SYS_TICK_PERIOD);
    }
    if (sched.usage.cur_ticks >= sched.usage.min_ticks)
    {
    	sched.usage.usage = sched.usage.run_count / sched.usage.min_ticks;
      
    	sched.usage.run_count = 0;
    	sched.usage.cur_ticks = 0;
    }
}

/**
  * This function will check if the highest priority of thread scheduler changed
  */
INLINE void sched_proc_current_thread(void)
{   
    --sched.current_thread->cur_ticks;
    if (!sched.current_thread->cur_ticks)
    {
        /* remove the thread from the thread-ready group */
        sched_set_thread_ready(sched.current_thread);
    }  
}

/**
 * This function is the entry function of scheduler timer timeout
 */
void sched_proc(void)
{
    phys_reg_t temp;
    
    SCHED_LOCK_PROC();
    
    temp = hw_interrupt_suspend();
    
    /* check and wakeup the sleeping thread */
    sched_wakeup_thread();
    
    sched_proc_current_thread();
    
    sched_run();
    
    sched_cpu_usage();
    
    hw_interrupt_recover(temp);
}

/**
 * This function will init the system scheduler
 *
 * @return the result of initing the scheduler
 */
err_t sched_init(void)
{
    int i;
    
    /* clear the scheduler structure */
    memset(&sched, 0, sizeof(struct sched));
    
    /* init all list of scheduler */
    for( i = 0; i < PTHREAD_PRIORITY_MAX + 1; i++ )
    {
        list_init( &sched.thread_ready_table[i] );
    }
    list_init(&sched.thread_list);
    list_init(&sched.thread_sleep_list);
    list_init(&sched.thread_delete_list);
    
    /* set cpu usage remark cycle */
    sched.usage.min_ticks = SCHED_CYCLE / RTOS_SYS_TICK_PERIOD;
    
#if USING_KERNEL_SECTION
    scheduler_privilege_mode = 1;
#endif
    
    return 0;
}

/*
 * This function will insert the thread into the systme register list
 */
void sched_insert_thread(os_pthread_t *thread)
{ 
    list_insert_tail(&sched.thread_list, &thread->tlist);
    sched_set_thread_ready(thread);
}

/**
 * This function will insert the thread into the thread-ready list and set the
 * group
 */
void sched_set_thread_ready(os_pthread_t *thread)
{ 
    list_remove_node(&thread->list);

    list_insert_tail(&sched.thread_ready_table[thread->cur_prio], &thread->list);
    sched.thread_ready_group |= (1 << thread->cur_prio);
    thread->status = PTHREAD_STATE_READY;
}

/**
 * This function will insert the thread into the thread-ready list and set the
 * status of the thread to be interruptible
 */
void sched_set_thread_int(os_pthread_t *thread)
{ 
    list_remove_node(&thread->list);

    list_insert_tail(&sched.thread_ready_table[thread->cur_prio], &thread->list);
    sched.thread_ready_group |= (1 << thread->cur_prio);
    thread->status = PTHREAD_STATE_INT;
}

/**
 * This function will suspend the thread and move from the thread-ready table 
 * and reset the group scheduler_suspend_thread
 */
void sched_set_thread_suspend(os_pthread_t *thread)
{ 
    list_remove_node(&thread->list); 
    if (list_is_empty(&sched.thread_ready_table[thread->cur_prio]))
    	sched.thread_ready_group &= ~(1 << (thread->cur_prio));
    
    thread->status = PTHREAD_STATE_SUSPEND;
}

/*
 * his function will let the thread sleep
 */
void sched_set_thread_sleep(os_pthread_t *thread)
{
    list_remove_node(&thread->list);
    if (list_is_empty(&sched.thread_ready_table[thread->cur_prio]))
    	sched.thread_ready_group &= ~(1 << (thread->cur_prio));
    
    list_insert_tail(&sched.thread_sleep_list, &thread->list);
    thread->status = PTHREAD_STATE_SLEEP;
}

/*
 * This function will let the thread closed
 */
void sched_set_thread_close(os_pthread_t *thread)
{
    list_remove_node(&thread->list);   
    if (list_is_empty(&sched.thread_ready_table[thread->cur_prio]))
    	sched.thread_ready_group &= ~(1 << (thread->cur_prio));
    
    thread->status = PTHREAD_STATE_CLOSED;
}

/*
 * This function will reclaim the thread closed
 */
void sched_reclaim_thread(os_pthread_t *thread)
{
    sched_set_thread_close(thread);
    
    list_insert_tail(&sched.thread_delete_list, &thread->list);
}

/**
 * This function will return the current thread point
 *
 * return the current thread point
 */
os_pthread_t* get_current_thread(void)
{
    return sched.current_thread;
}

/**
  * This function will delete the thread from the scheduler table
  *
  * @param thread the target thread
  *
  * return the result
  */
err_t sched_delete_thread(os_pthread_t *thread)
{
    phys_reg_t temp;  
  
    /* suspend the hardware interrupt for atomic operation */
    temp = hw_interrupt_suspend();
    
    sched_set_thread_close(thread);
    list_remove_node(&thread->tlist);
    sched_reclaim_thread(thread);
    
    hw_interrupt_recover(temp);
    
    sched_run();
  
    return 0;
}


/**
 * sched_yield - This function will let the current thread jump into the tail   
 *               of the thread-ready list
 */
void sched_yield(void)
{
    phys_reg_t temp;
  
    /* suspend the hardware interrupt for atomic operation */
    temp = hw_interrupt_suspend();
    
    sched_set_thread_ready(get_current_thread());
       
    hw_interrupt_recover(temp);
    
    sched_run();
}

/**
  * the function will suspend the scheduler
  *
  * @return the previous scheduler lock status
  */
os_u32 sched_suspend(void)
{
    phys_reg_t temp;
    os_u32 lcoked;
  
    temp = hw_interrupt_suspend();
  
    lcoked = sched.locked;
    sched.locked = SCHEDULER_IS_LOCKED;
  
    hw_interrupt_recover(temp);
  
    return lcoked;
}

/**
  * the function will recover the scheduler status
  *
  * @param state the status of the scheduler lock
  */
void sched_recover(os_u32 state)
{
    phys_reg_t temp;
  
    temp = hw_interrupt_suspend();
  
    sched.locked = state;
  
    hw_interrupt_recover(temp);
}

/**
 * This function will return the current CPU usage
 *
 * @return the current CPU usage
 */
os_u16 get_current_usage(void)
{
    return sched.usage.usage;
}

/**
  * the function will report the current thread
  */
void sched_status_report(void)
{
    os_pthread_t *pthread = get_current_thread();
    
    if (!pthread->name[0])
        printk("current thread id is 0x%08x.\r\n", pthread);
    else
        printk("current thread name is %s.\r\n", pthread->name);
}

/**
  * the function will report the current thread
  */
int __sched_report_signal(os_pthread_t *thread,
                          void *(*start_routine)(void*), 
                          void *RESTRICT arg)
{
    phys_reg_t temp;
    
    if (thread->status == PTHREAD_STATE_INT)
        return 0;
  
    temp = hw_interrupt_suspend();
    
    __pthread_int_init(thread, start_routine, arg);
    
    sched_set_thread_int(thread);
    
    sched_run();
  
    hw_interrupt_recover(temp);
    
    return 0;
}

/*@}*/
