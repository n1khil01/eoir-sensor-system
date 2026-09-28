/*
 * eoir-sensor-system — Week 6 FreeRTOS module
 * ---------------------------------------------------------------------------
 * Standalone FreeRTOS demonstration of the time-critical sensor-polling
 * concept from PLAN.md Week 6, per the plan's explicit fallback: "If full
 * hardware integration threatens the timeline, build this as a standalone
 * FreeRTOS module that demonstrates the concepts without wiring it into the
 * main pipeline." No STM32 board is used here — the target is QEMU's
 * mps2-an385 machine (a real Cortex-M3), which runs genuine FreeRTOS
 * scheduling and interrupt handling, just without physical hardware.
 *
 * What this demonstrates, mapped to the Week 6 metrics in PLAN.md:
 *
 *   1. Bounded scheduling jitter on a periodic polling task, worst case
 *      over >=10,000 periods. TIMER0 fires a hardware interrupt at a fixed
 *      rate; the ISR timestamps itself against a free-running cycle counter
 *      (see prvCycleCounterInit's comment for why that counter is TIMER1
 *      rather than the DWT the plan names — QEMU's mps2-an385 model doesn't
 *      implement DWT) and gives a binary semaphore. Jitter is the deviation
 *      of the actual inter-ISR interval from the programmed reload value.
 *
 *   2. Worst-case task response latency under contention. vPollTask blocks
 *      on the semaphore at the highest application priority and timestamps
 *      itself immediately on waking; the ISR-to-task latency is measured
 *      both with and without a lower-priority CPU hog task running, to show
 *      that priority-based preemption keeps the response latency bounded
 *      regardless of background load.
 *
 *   3. Stack and RAM headroom via uxTaskGetStackHighWaterMark, sized from
 *      the measured value rather than guesswork.
 *
 * Run under QEMU with no hardware attached ("make run" does this):
 *   qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -icount shift=auto \
 *     -kernel build/rtos_week6.elf -monitor none -nographic -serial stdio
 *
 * The -icount flag matters more than it looks. Without it, QEMU's virtual
 * clock tracks host wall-clock time, so when the host OS descheduled the
 * QEMU process (this ran on a shared dev machine, not an idle one) the
 * guest's own hardware timer appeared to jitter by over a millisecond on a
 * 500us period -- an artifact of the host, not the target. -icount ties the
 * guest's virtual time to instructions retired instead, decoupling it from
 * host scheduling, and the same build then reports worst-case jitter in the
 * tens of nanoseconds. Both raw logs are kept in benchmarks/ (
 * week6_qemu_wallclock_timing.txt vs week6_qemu_icount_timing.txt) because
 * the discrepancy between them is itself the more interesting finding.
 *
 * Vendored board-support files (CMSIS headers, mps2_m3.ld, startup_gcc.c,
 * printf-stdarg.c) come from the FreeRTOS/FreeRTOS project's
 * CORTEX_MPS2_QEMU_IAR_GCC demo (MIT licensed) — see freertos/README.md.
 */

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "CMSDK_CM3.h"

/* --------------------------------------------------------------------- */
/* Board bring-up: UART for printf (printf-stdarg.c writes UART0 directly). */

#define UART0_ADDRESS    ( 0x40004000UL )
#define UART0_STATE      ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) )
#define UART0_CTRL       ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) )
#define UART0_BAUDDIV    ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) )

static void prvUARTInit( void )
{
    UART0_BAUDDIV = 16;
    UART0_CTRL = 1;
}

/* --------------------------------------------------------------------- */
/* Cycle-accurate timestamp source.
 *
 * PLAN.md names the DWT cycle counter (DWT->CYCCNT) as the fallback
 * instrument when no logic analyzer is available. QEMU's mps2-an385 CPU
 * model does not implement the DWT unit, though (CoreDebug->DEMCR and
 * DWT->CTRL both read back 0 after being written, confirmed by probing them
 * at boot) — a real Cortex-M3, and QEMU machine models that do implement
 * DWT, would make DWT->CYCCNT usable as-is.
 *
 * TIMER1 (otherwise unused here) is a real, QEMU-modeled CMSDK peripheral,
 * so it stands in as a free-running cycle counter instead: reload it with
 * the maximum count and no interrupt enabled, and let it count down
 * continuously at the CPU clock. Negating the down-counter's value turns it
 * into an ever-increasing timestamp with the same semantics DWT->CYCCNT
 * would have provided. */

static void prvCycleCounterInit( void )
{
    CMSDK_TIMER1->INTCLEAR = ( 1UL << 0 );
    CMSDK_TIMER1->RELOAD = 0xFFFFFFFFUL;
    CMSDK_TIMER1->CTRL = ( 1UL << 0 ); /* Enable timer; interrupt left disabled. */
}

static inline uint32_t prvCycleCount( void )
{
    return ~CMSDK_TIMER1->VALUE;
}

/* --------------------------------------------------------------------- */
/* TIMER0: the periodic "sensor ready" interrupt source standing in for the
 * time-critical polling event a real MCU would take from a sensor's data-
 * ready line. Configured for a fixed period so jitter is measurable against
 * a known-good reload value. */

/* 2 kHz -> 500 us nominal period, comfortably faster than the MLX90640's own
 * cadence so this demonstrates scheduling determinism independent of any
 * particular sensor's timing. */
#define TIMER0_FREQUENCY_HZ    ( 2000UL )

static SemaphoreHandle_t xPollSemaphore;
static volatile uint32_t ulIsrCycleStamp;
static volatile uint32_t ulIsrSequence;

void TIMER0_Handler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    CMSDK_TIMER0->INTCLEAR = ( 1UL << 0 );

    ulIsrCycleStamp = prvCycleCount();
    ulIsrSequence++;

    xSemaphoreGiveFromISR( xPollSemaphore, &xHigherPriorityTaskWoken );
    portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

static void prvTimerInit( void )
{
    CMSDK_TIMER0->INTCLEAR = ( 1UL << 0 );
    CMSDK_TIMER0->RELOAD = ( configCPU_CLOCK_HZ / TIMER0_FREQUENCY_HZ );
    CMSDK_TIMER0->CTRL = ( ( 1UL << 3 ) |  /* Enable timer interrupt. */
                           ( 1UL << 0 ) ); /* Enable timer. */

    NVIC_SetPriority( TIMER0_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY );
    NVIC_EnableIRQ( TIMER0_IRQn );
}

/* Unused on this board but referenced by startup_gcc.c's vector table. */
void TIMER1_Handler( void )
{
}

/* --------------------------------------------------------------------- */
/* Task priorities. configMAX_PRIORITIES == 5, so valid priorities are 0..4. */

#define PRIO_POLL_TASK     ( 4 )   /* Highest application priority. */
#define PRIO_REPORT_TASK   ( 2 )   /* Prints results; well below the poll task. */
#define PRIO_LOAD_TASK     ( 1 )   /* Deliberate low-priority CPU hog. */

#define SAMPLE_COUNT       ( 10000UL )
#define WARMUP_SAMPLES     ( 8UL )     /* Discard the first few periods:
                                         * one full reload interval hasn't
                                         * elapsed yet when they fire. */

/* Stats are accumulated in raw DWT cycles, not microseconds: at 25 MHz a
 * microsecond is only 25 cycles, so converting before comparing throws away
 * exactly the resolution this measurement needs. Cycles are converted to
 * nanoseconds (exact at this clock: 40 ns/cycle) only for the final report. */
typedef struct
{
    uint32_t period_worst_cycles;
    uint32_t response_worst_cycles;
    uint64_t response_sum_cycles;
    uint32_t samples;
    UBaseType_t stack_high_water_mark;
} PollStats_t;

static PollStats_t xBaseline;
static PollStats_t xUnderLoad;

static TaskHandle_t xPollTaskHandle;
static TaskHandle_t xLoadTaskHandle;
static volatile uint8_t ucLoadTaskShouldRun = 0;
static volatile uint8_t ucRunPhase = 0; /* 0 = baseline, 1 = under load, 2 = done */

static void vPollTask( void *pvParameters )
{
    ( void ) pvParameters;

    uint32_t ulLastIsrStamp = 0;
    uint32_t ulExpectedPeriodCycles = configCPU_CLOCK_HZ / TIMER0_FREQUENCY_HZ;
    uint32_t ulLocalSeqSeen = 0;

    for( ;; )
    {
        xSemaphoreTake( xPollSemaphore, portMAX_DELAY );
        uint32_t ulWakeCycle = prvCycleCount();
        uint32_t ulIsrStamp = ulIsrCycleStamp;
        uint32_t ulSeq = ulIsrSequence;

        if( ulSeq == ulLocalSeqSeen )
        {
            /* Spurious wake without a new ISR sample; skip this iteration. */
            continue;
        }
        ulLocalSeqSeen = ulSeq;

        /* Response latency: time from the ISR's own timestamp to this task
         * actually running, in cycles. This is the number that shows whether
         * priority-based preemption is doing its job under load. */
        uint32_t ulResponseCycles = ulWakeCycle - ulIsrStamp;

        PollStats_t *pxStats = ( ucRunPhase == 0 ) ? &xBaseline : &xUnderLoad;

        if( ulLastIsrStamp != 0 )
        {
            uint32_t ulActualPeriodCycles = ulIsrStamp - ulLastIsrStamp;
            uint32_t ulDeviationCycles = ( ulActualPeriodCycles > ulExpectedPeriodCycles )
                                        ? ( ulActualPeriodCycles - ulExpectedPeriodCycles )
                                        : ( ulExpectedPeriodCycles - ulActualPeriodCycles );

            if( pxStats->samples >= WARMUP_SAMPLES && ulDeviationCycles > pxStats->period_worst_cycles )
            {
                pxStats->period_worst_cycles = ulDeviationCycles;
            }
        }
        ulLastIsrStamp = ulIsrStamp;

        if( pxStats->samples >= WARMUP_SAMPLES )
        {
            if( ulResponseCycles > pxStats->response_worst_cycles )
            {
                pxStats->response_worst_cycles = ulResponseCycles;
            }
            pxStats->response_sum_cycles += ulResponseCycles;
        }
        pxStats->samples++;

        if( pxStats->samples >= ( SAMPLE_COUNT + WARMUP_SAMPLES ) )
        {
            pxStats->stack_high_water_mark = uxTaskGetStackHighWaterMark( NULL );

            if( ucRunPhase == 0 )
            {
                /* Baseline phase complete: start the CPU-hog task and begin
                 * the under-load phase with fresh counters. */
                ucRunPhase = 1;
                ulLastIsrStamp = 0;
                ucLoadTaskShouldRun = 1;
                vTaskResume( xLoadTaskHandle );
            }
            else if( ucRunPhase == 1 )
            {
                ucLoadTaskShouldRun = 0;
                ucRunPhase = 2;
            }
        }
    }
}

/* Deliberate low-priority CPU hog: never blocks voluntarily, so it only
 * runs when the scheduler has nothing higher-priority ready — exactly the
 * "contention" the response-latency measurement is meant to stress. */
static void vLoadTask( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( !ucLoadTaskShouldRun )
        {
            vTaskSuspend( NULL );
            continue;
        }
        /* Busy-spin. Deliberately no vTaskDelay/yield: this is the
         * lower-priority background load the poll task must preempt. */
        __asm volatile ( "nop" );
    }
}

/* 25 MHz -> exactly 40 ns/cycle, so this conversion is exact, not rounded. */
#define NS_PER_CYCLE    ( 1000000000UL / configCPU_CLOCK_HZ )

static void prvPrintStats( const char *pcLabel, const PollStats_t *pxStats )
{
    uint32_t ulMeanCycles = ( uint32_t ) ( pxStats->response_sum_cycles / pxStats->samples );

    printf( "\r\n-- %s (%u periods, %u us nominal) --\r\n",
            pcLabel, ( unsigned int ) pxStats->samples,
            ( unsigned int ) ( 1000000UL / TIMER0_FREQUENCY_HZ ) );
    printf( "  worst-case scheduling jitter : %u cycles (%u ns)\r\n",
            ( unsigned int ) pxStats->period_worst_cycles,
            ( unsigned int ) ( pxStats->period_worst_cycles * NS_PER_CYCLE ) );
    printf( "  response latency (ISR->task) : mean=%u cycles (%u ns)  worst=%u cycles (%u ns)\r\n",
            ( unsigned int ) ulMeanCycles, ( unsigned int ) ( ulMeanCycles * NS_PER_CYCLE ),
            ( unsigned int ) pxStats->response_worst_cycles,
            ( unsigned int ) ( pxStats->response_worst_cycles * NS_PER_CYCLE ) );
    printf( "  poll task stack high-water   : %u words free\r\n",
            ( unsigned int ) pxStats->stack_high_water_mark );
}

static void vReportTask( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( ucRunPhase == 2 )
        {
            printf( "\r\n=== eoir-sensor-system Week 6 FreeRTOS results ===\r\n" );
            printf( "TIMER0 period target: %u us (%u Hz), CPU clock: %u Hz\r\n",
                    ( unsigned int ) ( 1000000UL / TIMER0_FREQUENCY_HZ ),
                    ( unsigned int ) TIMER0_FREQUENCY_HZ,
                    ( unsigned int ) configCPU_CLOCK_HZ );
            prvPrintStats( "Baseline (poll task alone)", &xBaseline );
            prvPrintStats( "Under contention (low-priority CPU hog running)", &xUnderLoad );
            printf( "\r\n=== done ===\r\n" );

            for( ;; )
            {
                /* Halt here; QEMU run is expected to be stopped externally
                 * once results are captured from the serial log. */
                vTaskDelay( portMAX_DELAY );
            }
        }
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

/* --------------------------------------------------------------------- */

int main( void )
{
    prvUARTInit();
    prvCycleCounterInit();

    printf( "\r\neoir-sensor-system: Week 6 FreeRTOS module booting (QEMU mps2-an385)\r\n" );

    xPollSemaphore = xSemaphoreCreateBinary();
    configASSERT( xPollSemaphore != NULL );

    xTaskCreate( vPollTask, "Poll", configMINIMAL_STACK_SIZE, NULL, PRIO_POLL_TASK, &xPollTaskHandle );
    xTaskCreate( vLoadTask, "Load", configMINIMAL_STACK_SIZE, NULL, PRIO_LOAD_TASK, &xLoadTaskHandle );
    xTaskCreate( vReportTask, "Report", configMINIMAL_STACK_SIZE * 2, NULL, PRIO_REPORT_TASK, NULL );

    /* Load task starts suspended; vPollTask resumes it once the baseline
     * phase has collected enough samples. */
    vTaskSuspend( xLoadTaskHandle );

    prvTimerInit();

    vTaskStartScheduler();

    /* Only reached if there was insufficient heap to start the scheduler. */
    for( ;; )
    {
    }
    return 0;
}

/* --------------------------------------------------------------------- */
/* FreeRTOS hooks. */

void vApplicationMallocFailedHook( void )
{
    printf( "\r\n\r\nMalloc failed\r\n" );
    portDISABLE_INTERRUPTS();
    for( ;; )
    {
    }
}

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pxTask;
    printf( "\r\n\r\nStack overflow in %s\r\n", pcTaskName );
    portDISABLE_INTERRUPTS();
    for( ;; )
    {
    }
}

void vAssertCalled( const char *pcFileName, uint32_t ulLine )
{
    printf( "ASSERT! %s:%u\r\n", pcFileName, ( unsigned int ) ulLine );
    portDISABLE_INTERRUPTS();
    for( ;; )
    {
    }
}

/* configSUPPORT_STATIC_ALLOCATION is on for the idle task's own use inside
 * the kernel port; provide the required static buffers. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                     StackType_t **ppxIdleTaskStackBuffer,
                                     uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
