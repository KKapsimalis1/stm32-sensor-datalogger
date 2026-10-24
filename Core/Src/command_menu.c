#include "command_menu.h"
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "bme280.h"
#include "mpu6050.h"

#define RX_BUF_SIZE 64

static UART_HandleTypeDef *menu_huart;
static uint8_t rx_byte;
static char line_buf[RX_BUF_SIZE];
static volatile uint16_t line_idx = 0;
static volatile uint8_t line_ready = 0;

static void print_prompt(void) {
    printf("\r\n> ");
}

static void print_help(void) {
    printf("\r\nΔιαθέσιμες εντολές:\r\n");
    printf("  help        - εμφανίζει αυτό το menu\r\n");
    printf("  led on      - ανάβει το LD2\r\n");
    printf("  led off     - σβήνει το LD2\r\n");
    printf("  status      - εμφανίζει tick count\r\n");
    printf("  bme         - διαβάζει raw τιμές από BME280\r\n");
    printf("  mpu         - διαβάζει raw accel/gyro από MPU6050\r\n");
    printf("  scan        - ψάχνει συσκευές στο I2C bus\r\n");
}

void CommandMenu_Init(UART_HandleTypeDef *huart) {
    menu_huart = huart;
    printf("\r\n=== STM32 Data Logger - Command Menu ===\r\n");
    print_help();
    print_prompt();
    HAL_UART_Receive_IT(menu_huart, &rx_byte, 1);
}

/* Καλείται αυτόματα από το HAL όταν έρθει byte */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == menu_huart->Instance) {
        if (rx_byte == '\r' || rx_byte == '\n') {
            line_buf[line_idx] = '\0';
            line_ready = 1;
        } else if (line_idx < RX_BUF_SIZE - 1) {
            line_buf[line_idx++] = rx_byte;
            HAL_UART_Transmit(menu_huart, &rx_byte, 1, 10); // echo
        }
        HAL_UART_Receive_IT(menu_huart, &rx_byte, 1); // ξανά-άκουσε
    }
}

static void handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print_help();
    } else if (strcmp(cmd, "led on") == 0) {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        printf("\r\nLED ON");
    } else if (strcmp(cmd, "led off") == 0) {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        printf("\r\nLED OFF");
    } else if (strcmp(cmd, "status") == 0) {
        printf("\r\nTick: %lu ms", HAL_GetTick());
    } else if (strcmp(cmd, "bme") == 0) {
    	int32_t raw_t, raw_p, raw_h;
    	BME280_Status st = BME280_ReadRaw(&raw_t, &raw_p, &raw_h);
    	if (st == BME280_OK) {
          printf("\r\nraw_temp=%ld  raw_press=%ld  raw_hum=%ld", raw_t, raw_p, raw_h);
    	} else {
    	  printf("\r\nBME280 read error (code %d) - ελεγξε συνδεση/address", st);
        }
    } else if (strcmp(cmd, "mpu") == 0) {
        MPU6050_RawData d;
        MPU6050_Status st = MPU6050_ReadRaw(&d);
        if (st == MPU6050_OK) {
            printf("\r\naccel=(%d,%d,%d) gyro=(%d,%d,%d)",
                   d.accel_x, d.accel_y, d.accel_z,
                   d.gyro_x, d.gyro_y, d.gyro_z);
        } else {
            printf("\r\nMPU6050 read error (code %d)", st);
        }
    } else if (strcmp(cmd, "scan") == 0) {
		extern I2C_HandleTypeDef hi2c1;
		printf("\r\nI2C scan:");
		for (uint8_t a = 0x08; a < 0x78; a++) {
			if (HAL_I2C_IsDeviceReady(&hi2c1, a << 1, 1, 10) == HAL_OK) {
				printf("\r\n  found 0x%02X", a);
			}
		}
		printf("\r\nscan done");

    } else if (strlen(cmd) > 0) {
        printf("\r\nΆγνωστη εντολή: '%s' (γράψε 'help')", cmd);
    }
}

void CommandMenu_Process(void) {
    if (line_ready) {
        handle_command(line_buf);
        line_idx = 0;
        line_ready = 0;
        print_prompt();
    }
}
