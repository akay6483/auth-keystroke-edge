/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "i2c-lcd.h"
#include "sha256.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char current_password[] = "1234";
char input_buffer[5];
int input_index = 0;

typedef enum {
    LOCKED,
    UNLOCKED,
    CHANGING_PW
} LockState;

LockState system_state = LOCKED;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

#define EEPROM_ADDR 0xA0
#define PASS_MEM_ADDR 0x0000
#define MAGIC_BYTE 0x5A

#define SYSTEM_SALT "fP#fSDYi047UD9"

uint8_t current_hash[32];
uint8_t input_hash[32];
char salted_buffer[64];

void save_hash(uint8_t* new_hash) {
    uint8_t buffer[33];
    buffer[0] = MAGIC_BYTE;
    memcpy(&buffer[1], new_hash, 32);

    // Spoon-feed the EEPROM to prevent Page Wrapping
    for(int i = 0; i < 33; i++) {
            HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, PASS_MEM_ADDR + i, I2C_MEMADD_SIZE_16BIT, &buffer[i], 1, 100);
            HAL_Delay(10); // Changed from 5 to 10 to safely beat the 6ms Proteus limit!
    }
}

void load_eeprom(uint8_t* buffer) {
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, PASS_MEM_ADDR, I2C_MEMADD_SIZE_16BIT, buffer, 33, 100);
}

void load_hash(uint8_t* hash_buf) {
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, PASS_MEM_ADDR, I2C_MEMADD_SIZE_16BIT, hash_buf, 32, 100);
}

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

char keypad_map[3][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'}
};

char scan_keypad(void) {
    for (int i = 0; i < 3; i++) {
        // Set all rows HIGH
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_SET);
        // Pull the current row LOW
        HAL_GPIO_WritePin(GPIOB, (GPIO_PIN_0 << i), GPIO_PIN_RESET);

        // Check Columns
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) {
            while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET);
            return keypad_map[i][0];
        }
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
            while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET);
            return keypad_map[i][1];
        }
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
            while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET);
            return keypad_map[i][2];
        }
    }
    return 0; // No key pressed
}

void save_password(char* new_pass) {

    HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, PASS_MEM_ADDR, I2C_MEMADD_SIZE_16BIT, (uint8_t*)new_pass, 5, 1000);
    HAL_Delay(10);
}

void load_password(char* pass_buf) {
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, PASS_MEM_ADDR, I2C_MEMADD_SIZE_16BIT, (uint8_t*)pass_buf, 5, 1000);
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  //HAL_UART_Transmit(&huart2, (uint8_t*)"System Boot\r\n", 13, 100);
  //lcd_init();
  //lcd_send_string("System Ready");
  //HAL_Delay(1000);

  uint8_t eeprom_buffer[33];
    load_eeprom(eeprom_buffer);

    // Check the Magic Byte
    if (eeprom_buffer[0] != MAGIC_BYTE) {
        printf("EEPROM Blank/Corrupt. Initiating Factory Reset...\r\n");

        // 1. Generate the hash for the default PIN "1234"
        sprintf(salted_buffer, "%s%s", SYSTEM_SALT, "1234");

        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (uint8_t*)salted_buffer, strlen(salted_buffer));
        sha256_final(&ctx, current_hash);

        // 2. Save the Magic Byte + Hash to the EEPROM
        save_hash(current_hash);

        printf("Factory Reset Complete: Default PW set to 1234\r\n");
    } else {
        // Magic Byte matched! Safely extract the 32-byte hash
        memcpy(current_hash, &eeprom_buffer[1], 32);
        printf("Boot: Password Hash loaded from EEPROM.\r\n");
    }

    // Ensure the door is physically locked and alarm is off on boot
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

    printf("\r\n--- SYSTEM LOCKED ---\r\nEnter 4-digit code:\r\n");

    printf("\r\n--- SYSTEM LOCKED ---\r\nEnter 4-digit code:\r\n");

    //lcd_clear();
    //lcd_send_string("Enter Password:");
    //lcd_send_cmd(0xC0);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  char key = scan_keypad();
	        if (key != 0) {
	        	if (system_state == LOCKED) {
	        	              input_buffer[input_index++] = key;
	        	              lcd_send_data('*');
	        	              printf("*"); // RESTORED: Terminal Lifeline

	        	              if (input_index == 4) {
	        	            	  input_buffer[4] = '\0';

	        	            	                    // --- CRYPTO: HASH THE INPUT ---
	        	            	                    sprintf(salted_buffer, "%s%s", SYSTEM_SALT, input_buffer);
	        	            	                    SHA256_CTX ctx;
	        	            	                    sha256_init(&ctx);
	        	            	                    sha256_update(&ctx, (uint8_t*)salted_buffer, strlen(salted_buffer));
	        	            	                    sha256_final(&ctx, input_hash);
	        	            	                    // ------------------------------

	        	            	                    lcd_clear();

	        	            	                    // Compare 32-byte hashes using memcmp
	        	            	                    if (memcmp(input_hash, current_hash, 32) == 0) {
	        	            	                        system_state = UNLOCKED;

	        	            	                        // HARDWARE ACTUATION: UNLOCK (Relay ON)
	        	            	                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

	        	            	                        lcd_send_string("ACCESS GRANTED");
	        	            	                        printf("\r\nACCESS GRANTED! Door Unlocked.\r\n");
	        	            	                        printf("Menu: 1:Lock | 9:Change PW\r\n");
	        	            	                        HAL_Delay(1500);

	        	            	                        lcd_clear();
	        	            	                        lcd_send_string("1:Lock 9:Change");
	        	            	                    } else {
	        	            	                        lcd_send_string("ACCESS DENIED");
	        	            	                        printf("\r\nACCESS DENIED!\r\n");

	        	            	                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);

	        	            	                        HAL_Delay(1500);

	        	            	                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

	        	            	                        lcd_clear();
	        	            	                        lcd_send_string("Enter Password:");
	        	            	                        lcd_send_cmd(0xC0);
	        	                  }
	        	                  input_index = 0;
	        	              }
	        	          }

	        	          else if (system_state == UNLOCKED) {
	        	        	  if (key == '1') {
	        	        	                    system_state = LOCKED;

	        	        	                    // HARDWARE ACTUATION: LOCK (Relay OFF)
	        	        	                    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);

	        	        	                    lcd_clear();
	        	        	                    lcd_send_string("Enter Password:");
	        	        	                    lcd_send_cmd(0xC0);
	        	        	                    printf("\r\n--- SYSTEM LOCKED ---\r\nEnter 4-digit code:\r\n");
	        	        	                }
	        	        	                else if (key == '9') {
	        	        	                    system_state = CHANGING_PW;
	        	        	                    input_index = 0;

	        	        	                    lcd_clear();
	        	        	                    lcd_send_string("New Password:");
	        	        	                    lcd_send_cmd(0xC0);
	        	        	                    printf("\r\n--- ADMIN MODE ---\r\nEnter NEW 4-digit code:\r\n");
	        	        	                }
	        	          }

	        	          else if (system_state == CHANGING_PW) {
	        	        	  input_buffer[input_index++] = key;
	        	        	                lcd_send_data(key); // Standard UI: show new digits briefly
	        	        	                printf("%c", key);

	        	        	                if (input_index == 4) {
	        	        	                    input_buffer[4] = '\0';

	        	        	                    // --- CRYPTO: HASH NEW PASSWORD & SAVE ---
	        	        	                    sprintf(salted_buffer, "%s%s", SYSTEM_SALT, input_buffer);
	        	        	                    SHA256_CTX ctx;
	        	        	                    sha256_init(&ctx);
	        	        	                    sha256_update(&ctx, (uint8_t*)salted_buffer, strlen(salted_buffer));
	        	        	                    sha256_final(&ctx, current_hash); // Save straight to memory array

	        	        	                    save_hash(current_hash); // Burn 32 bytes to EEPROM
	        	        	                    // ----------------------------------------

	        	        	                    system_state = UNLOCKED;

	        	        	                    lcd_clear();
	        	        	                    lcd_send_string("PW Updated!");
	        	        	                    printf("\r\nPassword Hash successfully updated in EEPROM!\r\n");
	        	        	                    printf("Menu: 1:Lock | 9:Change PW\r\n");
	        	        	                    HAL_Delay(1500);

	        	        	                    lcd_clear();
	        	        	                    lcd_send_string("1:Lock 9:Change");
	        	        	                    input_index = 0;
	        	              }
	        	          }
	        }


	        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);
	        HAL_Delay(20);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
