#include "i2c-lcd.h"
extern I2C_HandleTypeDef hi2c1;  // Uses the I2C1 handle from main.c

#define SLAVE_ADDRESS_LCD 0x40 // 0x4E is standard for Proteus PCF8574. (Sometimes 0x40)

void lcd_send_cmd (char cmd)
{
  char data_u, data_l;
  uint8_t data_t[4];
  data_u = (cmd&0xf0);
  data_l = ((cmd<<4)&0xf0);
  data_t[0] = data_u|0x0C;  // en=1, rs=0
  data_t[1] = data_u|0x08;  // en=0, rs=0
  data_t[2] = data_l|0x0C;  // en=1, rs=0
  data_t[3] = data_l|0x08;  // en=0, rs=0
  HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 100);
}

void lcd_send_data (char data)
{
  char data_u, data_l;
  uint8_t data_t[4];
  data_u = (data&0xf0);
  data_l = ((data<<4)&0xf0);
  data_t[0] = data_u|0x0D;  // en=1, rs=1
  data_t[1] = data_u|0x09;  // en=0, rs=1
  data_t[2] = data_l|0x0D;  // en=1, rs=1
  data_t[3] = data_l|0x09;  // en=0, rs=1
  HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 100);
}

void lcd_init (void)
{
  uint8_t data_t[2];

  // 1. Strict 8-bit Initialization (Single Nibbles Only)
  HAL_Delay(50);

  // Pulse 0x30
  data_t[0] = 0x3C; data_t[1] = 0x38;
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 2, 100);
  HAL_Delay(5);

  // Pulse 0x30 again
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 2, 100);
  HAL_Delay(1);

  // Pulse 0x30 a third time
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 2, 100);
  HAL_Delay(10);

  // Pulse 0x20 to safely enter 4-bit mode
  data_t[0] = 0x2C; data_t[1] = 0x28;
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 2, 100);
  HAL_Delay(10);

  // 2. Standard 4-bit commands (Safe to use dual-nibbles now)
  lcd_send_cmd(0x28); // Function set: 4-bit, 2 lines, 5x8 dots
  HAL_Delay(1);
  lcd_send_cmd(0x08); // Display off
  HAL_Delay(1);
  lcd_send_cmd(0x01); // Clear display
  HAL_Delay(5);       // CRITICAL FIX: Give Proteus >2ms to clear the screen
  lcd_send_cmd(0x06); // Entry mode set
  HAL_Delay(1);
  lcd_send_cmd(0x0C); // Display on, Cursor off, Blink off
}

void lcd_send_string (char *str)
{
  while (*str) lcd_send_data (*str++);
}

void lcd_clear (void)
{
	lcd_send_cmd(0x01); // Standard hardware clear command
	HAL_Delay(2);
}
