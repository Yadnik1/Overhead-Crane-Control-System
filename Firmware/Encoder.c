/* USER CODE BEGIN Header */

// Include necessary libraries and headers
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

/* USER CODE END Header */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Define addresses for master and slave
#define ADDRESS_MASTER 0x25D34478
#define ADDRESS_SLAVE  0x6DA0C59B

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

// Declare variables
NRF905_hw_t NRF905_hw;
NRF905_t NRF905;
int master;
int ret;
char buffer[64];
uint8_t nrf905_payload_buffer[NRF905_MAX_PAYLOAD];
int message;
int message_length;
bool button3_pressed = false;
uint32_t my_address;
uint32_t receiver_address;
bool button1_second_press = false;

// Declare variables for button control
volatile uint32_t button_check_counter = 0;
bool relay_enabled = false;
bool button1_first_press = true;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */

// Function to print to UART with formatting
void print_uart(const char *format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

// Function to read the state of the buttons and return a corresponding 32-bit pattern
uint32_t get_button_data(uint8_t button_num);

// Function to check the state of all buttons and transmit a 32-bit pattern based on the state
void check_buttons_and_transmit();

/* USER CODE END PFP */

// MAIN CODE
int main(void) {
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  HAL_TIM_Base_Start_IT(&htim3);
  MX_TIM1_Init();

  /* USER CODE BEGIN 2 */

  // Calculate a unique ID based on hardware
  uint32_t uid = 0x00;
  for (uint8_t i = 0; i < 3; ++i) {
    uid += *(uint32_t*) (UID_BASE + i * 4);
  }
  srand(uid);

  // Configure NRF905 hardware and initialize variables
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

  master = 1; // Set master to 1 for master mode, 0 for slave mode

  // Determine addresses based on master/slave mode
  if (master == 1) {
    my_address = ADDRESS_MASTER;
    receiver_address = ADDRESS_SLAVE;
  } else {
    my_address = ADDRESS_SLAVE;
    receiver_address = ADDRESS_MASTER;
  }

  if (master == 1) {
    print_uart("Mode: Master, TX, %08lX\r\n", my_address);
  } else {
    print_uart("Mode: Slave, RX, %08lX\r\n", my_address);
  }

  NRF905_init(&NRF905, &NRF905_hw);
  NRF905_set_listen_address(&NRF905, receiver_address);

  // Print configuration registers to UART
  print_uart("Reg conf: ");
  for (int i = 0; i < 10; ++i) {
    uint8_t d = NRF905_read_config_register(&NRF905, i);
    print_uart("%02X, ", d);
  }
  print_uart("\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  while (1) {
    /* USER CODE BEGIN WHILE */

    // Check buttons and transmit data
    if (button_check_counter >= 2) {
      check_buttons_and_transmit();
      button_check_counter = 0;
    }

    /* USER CODE END WHILE */
  }
  /* USER CODE BEGIN 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// Callback function for timer interrupt
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim == &htim3) {
    button_check_counter++;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */

/* USER CODE END 0 */

