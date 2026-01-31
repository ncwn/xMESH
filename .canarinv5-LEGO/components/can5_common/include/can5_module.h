/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_module.h
 * Descr: Interface class whiche specify the prototype of a module
 *        Every module is considered to be an indipended state machine 
 *        initialized with init() and runned by calling the process() method
 *******************************************************************************/

#ifndef __CAN5_MODULE_H__
#define __CAN5_MODULE_H__

#include <time.h>
#include <stdint.h>
#include "can5_error.h"
// #include "can5_events.h"
// #include "tools/sched/can5_scheduled_if.h"
// #include "tools/cfg/can5_config_interface.h"
// #include "tools/sleep/can5_sleepable_if.h"

// #define CAN5_MODULE_VALIDATE(mod)                                                  
//     static_assert(mod.module.init, "\"init\" function for mdule "mod" is not defined"); 
//     static_assert(mod.module.loop, "\"loop\" function for mdule "mod" is not defined");
//     static_assert(mod.module.uinit, "\"uinit\" function for mdule "mod" is not defined"); 
//     static_assert(mod.module.status_getstr, "\"status_getstr\" function for mdule "mod" is not defined"); 

#define CAN5_MODULE_INIT(mod)       mod.module.init                                                                        
#define CAN5_MODULE_UINIT(mod)      mod.module.uninit                                                                      
#define CAN5_MODULE_STAT_GET(mod)   mod.module.status_get   
#define CAN5_MODULE_RUN(mod)        mod.module.run   
#define CAN5_CHECK_SLEEPABLE(mod)   mod.module.is_sleepable()
#define CAN5_CHECK_SLEEPING(mod)    mod.module.is_sleeping()

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0                                                          
#define CAN5_MODULE_STAT_STR(mod)   mod.module.status_getstr     
#define CAN5_MODULE_EVT_STR(mod)    mod.module.evt_getstr     
#else
static char* _nostr(){return "";}
#define CAN5_MODULE_STAT_STR(mod) _nostr 




#define CAN5_MODULE_EVT_STR(mod)  _nostr
#endif       

// #define CAN5_MODULE_LOOP(mod)     mod.module.uinit;                                                                         

typedef int32_t can5_modstat_t;

typedef struct can5_module_s {

    /**
     * @brief   Init the module and its submodules
     * @details Registers to the system events of its interest
     * 
     * @return can5_err_t 
     */
    can5_err_t (*init)();

    /**
     * @brief   Uninit the module and its peripherals
     * 
     * @return can5_err_t 
     */
    can5_err_t (*uninit)();

    /**
     * @brief  Run the module main function
     * 
     * @return can5_err_t 
     */
    can5_err_t (*run)();

    /**
     * @brief Pre-sleep code
     */
     can5_err_t (*pre_sleep)();

    /**
     * @return Post-sleep code
     */
    can5_err_t (*post_sleep)();

    /**
     * @return Can this module sleep?
     */
    bool (*is_sleepable)();

    /**
     * @brief Is sleeping?
     */
    bool (*is_sleeping)();

    /**
     * @brief get the module status code
     *
     */

    int32_t (*status_get)();

    /**
     * @brief Is module ready?
     */
    bool (*is_active)();

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0                                                          

    /**
     * @brief Get the string tag for the status if available
     * 
     */
    const char* (*status_getstr)(int32_t);

    /**
     * @brief Get the string tag for the event if available
     * 
     */
    const char* (*evt_getstr)(int32_t);

#endif

} can5_module_t;


#endif // __CAN5_MODULE_H__