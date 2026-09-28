/* FreeRTOSConfig.h — eoir-sensor-system Week 6 FreeRTOS module.
 * Target: QEMU mps2-an385 (Cortex-M3), standalone (no STM32 hardware).
 * Adapted from the FreeRTOS/FreeRTOS CORTEX_MPS2_QEMU_IAR_GCC demo's
 * FreeRTOSConfig.h, trimmed to what this module needs.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION                     1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ( ( unsigned long ) 25000000 )
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 )
#define configMINIMAL_STACK_SIZE                 ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 32 * 1024 ) )
#define configMAX_TASK_NAME_LEN                  ( 16 )
#define configUSE_TRACE_FACILITY                  0
#define configUSE_16_BIT_TICKS                    0
#define configIDLE_SHOULD_YIELD                   1
#define configUSE_CO_ROUTINES                     0
#define configUSE_MUTEXES                         1
#define configUSE_RECURSIVE_MUTEXES               0
#define configCHECK_FOR_STACK_OVERFLOW            2
#define configUSE_MALLOC_FAILED_HOOK              1
#define configUSE_QUEUE_SETS                      0
#define configUSE_COUNTING_SEMAPHORES             1
#define configMAX_PRIORITIES                      ( 5UL )
#define configQUEUE_REGISTRY_SIZE                 4
#define configSUPPORT_STATIC_ALLOCATION           1
#define configSUPPORT_DYNAMIC_ALLOCATION          1

#define configUSE_TIMERS                          0

#define configUSE_TASK_NOTIFICATIONS              1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES     1

#define INCLUDE_vTaskPrioritySet                  0
#define INCLUDE_uxTaskPriorityGet                 0
#define INCLUDE_vTaskDelete                       0
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_uxTaskGetStackHighWaterMark       1
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_eTaskGetState                     1
#define INCLUDE_xTaskGetHandle                    0
#define INCLUDE_xSemaphoreGetMutexHolder           0

#define configKERNEL_INTERRUPT_PRIORITY           ( 255 )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY      ( 4 )
#define configUSE_PORT_OPTIMISED_TASK_SELECTION   1

#define configENABLE_BACKWARD_COMPATIBILITY       0

#ifndef __IASMARM__
void vAssertCalled( const char * pcFileName, uint32_t ulLine );
#define configASSERT( x )    if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ );
#endif

#endif /* FREERTOS_CONFIG_H */
