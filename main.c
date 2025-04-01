#include "FreeRTOS.h"
#include "math.h"
#include "stdio.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "task.h"
#include "stdlib.h"

#define CCM_RAM __attribute__((section(".ccmram")))
#define ms_TO_TICKS configTICK_RATE_HZ / 3000 // not sure why but this seems to be 3x slower than it should be
#define PI 3.141592653589793
#define TAU 6.283185307179586

// put all your task handlers here
TaskHandle_t animation_task;

// declare functions before the main so they can be referenced

void init_USART3(void);
void init_LEDS(void);
void init_button(void);

double randr()
{
    return (double)rand() / (double)RAND_MAX;
}

void animation(void* p);
void gamble();

int main(void)
{
    SystemInit();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    init_LEDS();
    init_button();

    // use this to create a new task
    xTaskCreate(animation, "animation_task", 256 / 4, NULL, 1, &animation_task);

    // this starts everything
    vTaskStartScheduler();

    for (;;);
}

// some stuff that does things ig?
// I think these are user-defined functions that FreeRTOS wants me to implement

void vApplicationTickHook(void) {};

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for(;;);
}

void vApplicationIdleHook(void) {};

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
    (void) pcTaskName;
    (void) pxTask;
    taskDISABLE_INTERRUPTS();
    for(;;);
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

// this is my stuff (mostly) that I understand

uint32_t SEEDED = 0;

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        if (!SEEDED)
        {
            SEEDED = 1;
            srand(xTaskGetTickCount());
        }

        gamble();

        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// these don't need mutex since they are being used on a monoprocessor between only 2 tasks
// and only one is actually changing the values anyway
double w = 0;
double dw = TAU / (6000 * 5999); // roughly 2pi/(6000*5999)
double dw_offset = 0;
int16_t state, slowmode, win, flash;

void animation(void* p)
{
    int16_t x, y, count;
    int16_t max_brightness = 666; // between 0 and 666
    double t = 0;

    for(;;)
    {
        x = max_brightness * cos(t);
        y = max_brightness * sin(t);

        // TIM4->CCRN is roughly the duty cycle of channel N
        if (win && w == 0)
        {
            if (flash)
            {
                TIM4->CCR1 = 0;
                TIM4->CCR2 = 0;
                TIM4->CCR3 = 0;
                TIM4->CCR4 = 0;
            } else {
                TIM4->CCR1 = max_brightness;
            }
            if (count > 2500)
            {
                flash = !flash;
                count = 0;
            }
            count++;
        } else {
            TIM4->CCR1 = (x > 0) * x;
            TIM4->CCR2 = (y > 0) * y;
            TIM4->CCR3 = -(x < 0) * x;
            TIM4->CCR4 = -(y < 0) * y;
        }

        t += w;

        // this makes the slowing animation only start at the green LED
        if (!slowmode && state)
        {
            if (fmod(t + w, TAU) <= 2 * w)
                slowmode = 1;
        }

        if (state && w != 0 && slowmode)
        {
            w -= dw + dw_offset;
            if (w < 0)
                w = 0;
        }

        vTaskDelay(1 * ms_TO_TICKS);
    }
}

// I think this sets up USART communication for debugging stuff?
void init_USART3(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);

    USART_InitStruct.USART_BaudRate = 115200;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStruct);
    USART_Cmd(USART3, ENABLE);
}

void init_LEDS(void)
{
    // these structs hold data needed to initialize
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    GPIO_InitTypeDef GPIO_LEDs;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // enable the leds as alternative function pins
    GPIO_LEDs.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_LEDs.GPIO_Mode = GPIO_Mode_AF;
    GPIO_LEDs.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_LEDs.GPIO_OType = GPIO_OType_PP;
    GPIO_LEDs.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_LEDs);

    // set the alternative function of the leds as connected to TIM4
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_TIM4);

    uint16_t PrescalerValue = 0;
    PrescalerValue = (uint16_t) ((SystemCoreClock / 2) / 21000000) - 1;

    TIM_TimeBaseStructure.TIM_Period = 665;
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    // tell TIM4 to treat all 4 channels (connected to leds) as pwm
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC3Init(TIM4, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC4Init(TIM4, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM4, ENABLE);

    // turn TIM4 on
    TIM_Cmd(TIM4, ENABLE);
}

void init_button(void)
{
    GPIO_InitTypeDef GPIO_Button;
    EXTI_InitTypeDef EXTI_Button;
    NVIC_InitTypeDef NVIC_Button;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    GPIO_Button.GPIO_Pin = GPIO_Pin_0;
    GPIO_Button.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Button.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Button.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &GPIO_Button);

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

    EXTI_Button.EXTI_Line = EXTI_Line0;
    EXTI_Button.EXTI_LineCmd = ENABLE;
    EXTI_Button.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_Button.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_Init(&EXTI_Button);

    NVIC_Button.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_Button.NVIC_IRQChannelPreemptionPriority = 0x06;
    NVIC_Button.NVIC_IRQChannelSubPriority = 0x06;
    NVIC_Button.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_Button);
}

void gamble()
{
    uint32_t phi;
    uint32_t T = 1024; // closest 2^n to 10^3 cause idk it feels right (and 10^6 breaks everything?)
    double spin_rate = TAU / 1000; // 2pi/1000
    double prob = 0.5; // probability between 0 and 1

    uint32_t current = xTaskGetTickCount() % T;
    phi = randr() * T * (1 - prob);
    if (state)
    {
        w = spin_rate;
        slowmode = 0;
        flash = 1;
    } else {
        if (current >= phi && current <= phi + (uint32_t)(prob * (double)T))
        {
            win = 1;
            dw_offset = 0;
        } else {
            win = 0;
            dw_offset = (dw / PI) * randr(); // using pi here should guarentee that it doesn't land on green (hopefully?)
        }
    }
    state = ! state;
}
