/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   16/10/2024
 ******************************************************************************
**/

#include "io_cfg.h"

void led_init() {
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_10 | LL_GPIO_PIN_11;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void led_pc6_on() {
    LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_6);
}

void led_pc6_off() {
    LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_6);
}

void led_pb10_on() {
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_10);
}

void led_pb10_off() {
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_10);
}

void led_pb11_on() {
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_11);
}

void led_pb11_off() {
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_11);
}

void usart1_init(uint32_t baudrate) {
    LL_USART_InitTypeDef USART_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* enable clock */
    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /* gpio config */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_9;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* usart1 config */
    USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
    USART_InitStruct.BaudRate = baudrate;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART1, &USART_InitStruct);
    LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
    LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
    LL_USART_DisableFIFO(USART1);
    LL_USART_ConfigAsyncMode(USART1);
    LL_USART_EnableIT_RXNE(USART1);

    /* enable interrupt */
    NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_EnableIRQ(USART1_IRQn);

    LL_USART_Enable(USART1);
    while((!(LL_USART_IsActiveFlag_TEACK(USART1))) || (!(LL_USART_IsActiveFlag_REACK(USART1))));
}

void usart1_put_char(uint8_t c) {
    while (!LL_USART_IsActiveFlag_TXE(USART1));

    LL_USART_TransmitData8(USART1, c);

    while (!LL_USART_IsActiveFlag_TC(USART1));
}

uint8_t usart1_get_char() {
    volatile uint8_t c = 0;

    while (!LL_USART_IsActiveFlag_RXNE(USART1));
    c =  LL_USART_ReceiveData8(USART1);

    return c;
}

void io_init() {
    /* led life init */
    led_init();
}
