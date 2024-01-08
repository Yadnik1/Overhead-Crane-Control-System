* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "NRF905.h"
#include <stdarg.h>
#include <stdbool.h>

/* Private variables ---------------------------------------------------------*/
NRF905_hw_t NRF905_hw;
NRF905_t NRF905;
int master;
int ret;
char buffer[64];
uint8_t nrf905_payload_buffer[NRF905_MAX_PAYLOAD];
int message;
int message_length;
uint32_t first_received = 0;
uint32_t my_address;
uint32_t receiver_address;
uint32_t previous_received_data;
uint32_t output_relaypattern;
uint8_t button3_state = 0;
uint8_t relay_pattern1_flag = 0;
#define ADDRESS_MASTER 0x25D34478
#define ADDRESS_SLAVE  0x6DA0C59B
uint32_t received_data;
uint32_t received_number;
uint32_t x, y, relaypattern;
bool button3_relay_state = false;
uint32_t button3_debounce_time = 0;
int counter;
uint32_t relay_state = 0;
bool button3_state = false;
bool interlock_5_6_state = false;
uint8_t receiver_state = 0; // 0: standby, 1: active

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void print_uart(const char *format, ...);
void standby_mode(void);
void active_mode(void);

/* Application entry point */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI2_Init();
    MX_TIM1_Init();
    MX_USART2_UART_Init();
    MX_TIM3_Init();

    // Send initial relay state
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);

    uint32_t relaypattern = 0x00000000;
    x = relaypattern;

    // Shift out the entire 32-bit pattern to the shift register
    for (int i = 0; i < 32; i++)
    {
        uint32_t currentBit = (x >> (31 - i)) & 0x01;
        HAL_GPIO_WritePin(CD4094_DAT_GPIO_Port, CD4094_DAT_Pin, (currentBit == 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(CD4094_CLK_GPIO_Port, CD4094_CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CD4094_CLK_GPIO_Port, CD4094_CLK_Pin, GPIO_PIN_RESET);
    }

    // Latch the data
    HAL_GPIO_WritePin(CD4094_STR_GPIO_Port, CD4094_STR_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CD4094_STR_GPIO_Port, CD4094_STR_Pin, GPIO_PIN_RESET);

    HAL_TIM_Base_Start_IT(&htim3);

    // Get a unique ID based on the device's UID
    uint32_t uid = 0x00;
    for (uint8_t i = 0; i < 3; ++i) {
        uid += *(uint32_t*) (UID_BASE + i * 4);
    }
    srand(uid);

    // Initialize NRF905 hardware configuration
    NRF905_hw.gpio[NRF905_HW_GPIO_TXEN].pin = TXEN_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_TXEN].port = TXEN_GPIO_Port;
    NRF905_hw.gpio[NRF905_HW_GPIO_TRX_EN].pin = TRX_CE_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_TRX_EN].port = TRX_CE_GPIO_Port;
    NRF905_hw.gpio[NRF905_HW_GPIO_PWR].pin = PWR_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_PWR].port = PWR_GPIO_Port;
    NRF905_hw.gpio[NRF905_HW_GPIO_CD].pin = CD_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_CD].port = CD_GPIO_Port;
    NRF905_hw.gpio[NRF905_HW_GPIO_AM].pin = 0;
    NRF905_hw.gpio[NRF905_HW_GPIO_AM].port = NULL;
    NRF905_hw.gpio[NRF905_HW_GPIO_DR].pin = DR_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_DR].port = DR_GPIO_Port;
    NRF905_hw.gpio[NRF905_HW_GPIO_CS].pin = SPI_CS_Pin;
    NRF905_hw.gpio[NRF905_HW_GPIO_CS].port = SPI_CS_GPIO_Port;

    NRF905_hw.tim = &htim1;
    NRF905_hw.spi = &hspi2;

    master = HAL_GPIO_ReadPin(MODE_GPIO_Port, MODE_Pin);

    if (master == 1) {
        my_address = ADDRESS_MASTER;
        receiver_address = ADDRESS_SLAVE;
        print_uart("Mode: Master, TX, %08lX\r\n", my_address);
    } else {
        my_address = ADDRESS_SLAVE;
        receiver_address = ADDRESS_MASTER;
        print_uart("Mode: Slave, RX, %08lX\r\n", my_address);
    }

    NRF905_init(&NRF905, &NRF905_hw);
    NRF905_set_listen_address(&NRF905, receiver_address);

    print_uart("Reg conf: ");
    for (int i = 0; i < 10; ++i) {
        uint8_t d = NRF905_read_config_register(&NRF905, i);
        print_uart("%02X, ", d);
    }
    print_uart("\r\n");

    standby_mode(); // Put NRF905 in standby mode

    while (1) {
        if (receiver_state == 1) {
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        } else if (receiver_state == 0) {
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
        }
        print_uart("Decoder working");
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
        NRF905_rx(&NRF905); // Receive data from NRF905 module
        // Process the received data here
    }
}

/* System Clock Configuration */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK| RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/* UART print function with variable arguments */
void print_uart(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

/* Put NRF905 in standby mode */
void standby_mode(void)
{
    NRF905_standby(&NRF905);
}

/* Put NRF905 in active mode */
void active_mode(void)
{
    NRF905_power_up(&NRF905);
}

/* EXTI rising edge interrupt callback for NRF905 DR pin */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == DR_Pin) {
        // Read received data into the payload buffer
        NRF905_read(&NRF905, nrf905_payload_buffer, sizeof(nrf905_payload_buffer));

        // Adjust indices and shift values according to the actual order of the received data
        relaypattern = ((uint32_t)nrf905_payload_buffer[0] << 24);
        relaypattern |= ((uint32_t)nrf905_payload_buffer[1] << 16);
        relaypattern |= ((uint32_t)nrf905_payload_buffer[2] << 8);
        relaypattern |= (uint32_t)nrf905_payload_buffer[3];

        print_uart("Data received: %02X %02X %02X %02X\r\n", nrf905_payload_buffer[0], nrf905_payload_buffer[1], nrf905_payload_buffer[2], nrf905_payload_buffer[3]);
        print_uart("relaypattern: %08lX\r\n", relaypattern);

        if (relaypattern & 0x80000000) { // button2 is pressed
            relay_state = 0; // Clear all bits in the relay_state to turn off all relays
            interlock_5_6_state = false; // Release interlock between buttons 5 and 6
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); // LED goes low
        } else if (relaypattern & 0x40000000) { // button1 is pressed
            relay_state |= 0x01; // Set the first bit in relay_state to turn on relay 1
            relaypattern &= ~0x40000000; // Remove button1 flag from relaypattern
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); // LED goes high
        } else if (relaypattern & 0x00000010) { // button5 is pressed
            if (!interlock_5_6_state) {
                relay_state |= 0x10; // Set the fifth bit in relay_state to turn on relay 5
                relaypattern &= ~0x00000010; // Remove button5 flag from relaypattern
            }
        } else if (relaypattern & 0x00000008) { // button6 is pressed
            if (!interlock_5_6_state) {
                relay_state |= 0x20; // Set the sixth bit in relay_state to turn on relay 6
                relaypattern &= ~0x00000008; // Remove button6 flag from relaypattern
            }
        } else if (relaypattern & 0x00000004) { // button3 is pressed
            button3_state = !button3_state; // Toggle button3 state
            if (button3_state) {
                relay_state |= 0x08; // Set the fourth bit in relay_state to turn on relay 4
            } else {
                relay_state &= ~0x08; // Clear the fourth bit in relay_state to turn off relay 4
            }
        }

        // Shift out the entire 32-bit relay_state pattern
        for (int i = 0; i < 32; i++) {
            uint32_t currentBit = (relay_state >> (31 - i)) & 0x01;

            HAL_GPIO_WritePin(CD4094_DAT_GPIO_Port, CD4094_DAT_Pin, (currentBit == 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
            HAL_GPIO_WritePin(CD4094_CLK_GPIO_Port, CD4094_CLK_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(CD4094_CLK_GPIO_Port, CD4094_CLK_Pin, GPIO_PIN_RESET);
        }

        // Latch the data
        HAL_GPIO_WritePin(CD4094_STR_GPIO_Port, CD4094_STR_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CD4094_STR_GPIO_Port, CD4094_STR_Pin, GPIO_PIN_RESET);
    }
}

