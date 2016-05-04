#include "hal.h"

#include "debug.h"
#include "init.h"

err_t hal_init(void)
{
    for (int order = 0; order < HAL_ORDER_MAX; order++)
    {
        hal_func_t *hal_func = HAL_FUNC_START_ADDR;
        
        for (int i = 0; i < HAL_FUNC_NUM; i++)
        {
            err_t ret;
        
            if (hal_func->order == order && (ret = hal_func->init()))
            {
                   printk("[%s] function initialized error, it is for [%s], return error code %d.\r\n", 
                   hal_func->name, hal_func->desc, ret);
            
                while(ret);
            }
            hal_func++;
        }
    }
  
    return 0;
}