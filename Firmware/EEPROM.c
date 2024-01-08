/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

/* Private typedef -----------------------------------------------------------*/

typedef struct {
    uint16_t Relay;
    uint16_t Toggle;
    uint16_t SelectorKey;
    uint16_t PairedKey;
    uint16_t Interlock;
    uint16_t Padding1; // First padding to align the structure
    uint16_t Padding2; // Second padding to align the structure
    uint16_t Padding3; // Third padding
} MemoryEntry;

/* Private define ------------------------------------------------------------*/

#define KEY1_ADDRESS_SECTOR1 0x0800F800
#define KEY2_ADDRESS_SECTOR1 0x0800F810
#define KEY3_ADDRESS_SECTOR1 0x0800F820

#define KEY1_ADDRESS_SECTOR2 0x0800F860
#define KEY2_ADDRESS_SECTOR2 0x0800F870
#define KEY3_ADDRESS_SECTOR2 0x0800F880

/* Private variables ---------------------------------------------------------*/

MemoryEntry key1_sector1 = {.Relay = 0x0000, .Toggle = 0x03, .SelectorKey = 0x08, .PairedKey = 0x09, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};
MemoryEntry key2_sector1 = {.Relay = 0x0001, .Toggle = 0x02, .SelectorKey = 0x03, .PairedKey = 0x01, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};
MemoryEntry key3_sector1 = {.Relay = 0x0000, .Toggle = 0x02, .SelectorKey = 0x05, .PairedKey = 0x03, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};

MemoryEntry key1_sector2 = {.Relay = 0x0000, .Toggle = 0x03, .SelectorKey = 0x08, .PairedKey = 0x09, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};
MemoryEntry key2_sector2 = {.Relay = 0x0001, .Toggle = 0x02, .SelectorKey = 0x03, .PairedKey = 0x01, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};
MemoryEntry key3_sector2 = {.Relay = 0x0000, .Toggle = 0x02, .SelectorKey = 0x05, .PairedKey = 0x03, .Interlock = 0x00, .Padding1 = 0x0000, .Padding2 = 0x0000, .Padding3 = 0x0000};

MemoryEntry ramSector1[3];
MemoryEntry ramSector2[3];

uint32_t currentSectorAddress = KEY1_ADDRESS_SECTOR1;

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);
void print_debug(char *str);
void ProcessATCommand(char *command);
void InitializeMemory(void);

/* Private user code ---------------------------------------------------------*/

void InitializeMemory(void) {

    // Write default values to Flash memory for Key1, Key2, and Key3 in Sector 1
    if (Flash_Write_Data(KEY1_ADDRESS_SECTOR1, (uint8_t*)&key1_sector1, sizeof(MemoryEntry)) != HAL_OK ||
        Flash_Write_Data(KEY2_ADDRESS_SECTOR1, (uint8_t*)&key2_sector1, sizeof(MemoryEntry)) != HAL_OK ||
        Flash_Write_Data(KEY3_ADDRESS_SECTOR1, (uint8_t*)&key3_sector1, sizeof(MemoryEntry)) != HAL_OK) {
        // Handle error in writing default values to Flash
        print_debug("Error initializing memory!\r\n");
    }

    // Write default values to Flash memory for Key1, Key2, and Key3 in Sector 2
    if (Flash_Write_Data(KEY1_ADDRESS_SECTOR2, (uint8_t*)&key1_sector2, sizeof(MemoryEntry)) != HAL_OK ||
        Flash_Write_Data(KEY2_ADDRESS_SECTOR2, (uint8_t*)&key2_sector2, sizeof(MemoryEntry)) != HAL_OK ||
        Flash_Write_Data(KEY3_ADDRESS_SECTOR2, (uint8_t*)&key3_sector2, sizeof(MemoryEntry)) != HAL_OK) {
        // Handle error in writing default values to Flash
        print_debug("Error initializing memory!\r\n");
    }
}

void ProcessATCommand(char *command) {
    uint32_t address;
    MemoryEntry *entry = NULL;
    MemoryEntry tempEntries[3];
    bool query = false;
    char keyName[10] = "";

    // Make a temporary copy of all keys in the current sector
    if (currentSectorAddress == KEY1_ADDRESS_SECTOR1) {
        Flash_Read_Data(KEY1_ADDRESS_SECTOR1, (uint8_t*)&tempEntries[0], sizeof(MemoryEntry));
        Flash_Read_Data(KEY2_ADDRESS_SECTOR1, (uint8_t*)&tempEntries[1], sizeof(MemoryEntry));
        Flash_Read_Data(KEY3_ADDRESS_SECTOR1, (uint8_t*)&tempEntries[2], sizeof(MemoryEntry));
    } else if (currentSectorAddress == KEY1_ADDRESS_SECTOR2) {
        Flash_Read_Data(KEY1_ADDRESS_SECTOR2, (uint8_t*)&tempEntries[0], sizeof(MemoryEntry));
        Flash_Read_Data(KEY2_ADDRESS_SECTOR2, (uint8_t*)&tempEntries[1], sizeof(MemoryEntry));
        Flash_Read_Data(KEY3_ADDRESS_SECTOR2, (uint8_t*)&tempEntries[2], sizeof(MemoryEntry));
    }
    if (strncmp(command, "AT: SECTOR 1\r\n", strlen("AT: SECTOR 1\r\n") - 1) == 0) {
        currentSectorAddress = KEY1_ADDRESS_SECTOR1;
        print_debug("Sector 1 selected.\r\n");
        return;
    } else if (strncmp(command, "AT: SECTOR 2\r\n", strlen("AT: SECTOR 2\r\n") - 1) == 0) {
        currentSectorAddress = KEY1_ADDRESS_SECTOR2;
        print_debug("Sector 2 selected.\r\n");
        return;
    }


    if (strncmp(command, "AT: KEY1:", 9) == 0) {
        entry = &tempEntries[0];
        address = currentSectorAddress;
        strcpy(keyName, "Key 1");
    } else if (strncmp(command, "AT: KEY2:", 9) == 0) {
        entry = &tempEntries[1];
        address = currentSectorAddress + sizeof(MemoryEntry);
        strcpy(keyName, "Key 2");
    } else if (strncmp(command, "AT: KEY3:", 9) == 0) {
        entry = &tempEntries[2];
        address = currentSectorAddress + 2 * sizeof(MemoryEntry);
        strcpy(keyName, "Key 3");
    } else if (strncmp(command, "AT: QUERY_KEY1", 14) == 0) {
        address = currentSectorAddress;
        query = true;
    } else if (strncmp(command, "AT: QUERY_KEY2", 14) == 0) {
        address = currentSectorAddress + sizeof(MemoryEntry);
        query = true;
    } else if (strncmp(command, "AT: QUERY_KEY3", 14) == 0) {
        address = currentSectorAddress + 2 * sizeof(MemoryEntry);
        query = true;
    } else {
        print_debug("Invalid AT Command!\r\n");
        return;
    }

    if (query) {
        MemoryEntry queryEntry;
        Flash_Read_Data(address, (uint8_t*)&queryEntry, sizeof(MemoryEntry));

        char buffer[200];
        sprintf(buffer, "Relay=%u, Toggle=%u, SelectorKey=%u, PairedKey=%u, Interlock=%u\r\n",
                queryEntry.Relay, queryEntry.Toggle, queryEntry.SelectorKey, queryEntry.PairedKey, queryEntry.Interlock);
        print_debug(buffer);
        return;
    }

    // Scan for individual fields and values
    char *param_position;
    char updateMessage[100];
    uint16_t oldRelay = entry->Relay;
    uint16_t oldToggle = entry->Toggle;
    uint16_t oldSelectorKey = entry->SelectorKey;
    uint16_t oldPairedKey = entry->PairedKey;
    uint16_t oldInterlock = entry->Interlock;

    param_position = strstr(command, "RELAY=");
    if (param_position) {
        sscanf(param_position, "RELAY=%hx", &entry->Relay);
        sprintf(updateMessage, "Relay updated across %s. Changed from 0x%04x to 0x%04x.\r\n", keyName, oldRelay, entry->Relay);
        print_debug(updateMessage);
    }

    param_position = strstr(command, "TOGGLE=");
    if (param_position) {
        sscanf(param_position, "TOGGLE=%hx", &entry->Toggle);
        sprintf(updateMessage, "Toggle updated across %s. Changed from 0x%04x to 0x%04x.\r\n", keyName, oldToggle, entry->Toggle);
        print_debug(updateMessage);
    }

    param_position = strstr(command, "SELECTORKEY=");
    if (param_position) {
        sscanf(param_position, "SELECTORKEY=%hx", &entry->SelectorKey);
        sprintf(updateMessage, "SelectorKey updated across %s. Changed from 0x%04x to 0x%04x.\r\n", keyName, oldSelectorKey, entry->SelectorKey);
        print_debug(updateMessage);
    }

    param_position = strstr(command, "PAIREDKEY=");
    if (param_position) {
        sscanf(param_position, "PAIREDKEY=%hx", &entry->PairedKey);
        sprintf(updateMessage, "PairedKey updated across %s. Changed from 0x%04x to 0x%04x.\r\n", keyName, oldPairedKey, entry->PairedKey);
        print_debug(updateMessage);
    }

    param_position = strstr(command, "INTERLOCK=");
    if (param_position) {
        sscanf(param_position, "INTERLOCK=%hx", &entry->Interlock);
        sprintf(updateMessage, "Interlock updated across %s. Changed from  0x%04x to 0x%04x.\r\n", keyName, oldInterlock, entry->Interlock);
        print_debug(updateMessage);
    }

    // Erase the Flash sector before writing new data
    if (currentSectorAddress == KEY1_ADDRESS_SECTOR1) {
        if (Flash_Erase_Sector(KEY1_ADDRESS_SECTOR1) != HAL_OK) {
            print_debug("Failed to erase Flash sector!\r\n");
            return;
        }
    } else if (currentSectorAddress == KEY1_ADDRESS_SECTOR2) {
        if (Flash_Erase_Sector(KEY1_ADDRESS_SECTOR2) != HAL_OK) {
            print_debug("Failed to erase Flash sector!\r\n");
            return;
        }
    }

    // Write all the keys back to Flash memory
    for (int i = 0; i < 3; i++) {
        uint32_t writeAddress = currentSectorAddress + i * sizeof(MemoryEntry);
        HAL_StatusTypeDef status = Flash_Write_Data(writeAddress, (uint8_t*)&tempEntries[i], sizeof(MemoryEntry));
        if (status != HAL_OK) {
            print_debug("Failed to update data in Flash!\r\n");
            return;
        }
    }
}

void print_debug(char *str) {
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

int main(void)
{
    /* MCU Configuration--------------------------------------------------------*/

    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* Initialize memory with default values */
    InitializeMemory();

    char rx_buffer[100];
    uint8_t received_byte;
    int rx_index = 0;

    while (1) {
        if (HAL_UART_Receive(&huart2, &received_byte, 1, HAL_MAX_DELAY) == HAL_OK) {
            if (received_byte == '\n') {
                rx_buffer[rx_index] = 0; // Null-terminate the string
                ProcessATCommand(rx_buffer);
                rx_index = 0; // Reset the buffer index
            } else {
                rx_buffer[rx_index++] = received_byte;
            }
        }
    }
}

void SystemClock_Config(void)
{
    /* Configure the system clock here */
}

void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
}

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number */
}
#endif /* USE_FULL_ASSERT */

