#include "command_menu.h"
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "bme280.h"
#include "mpu6050.h"
#include "stdlib.h"
#include "data_logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_config.h"
#include "sensor_task.h"

#define RX_BUF_SIZE 64

static TaskHandle_t cli_task;
static UART_HandleTypeDef *menu_huart;
static uint8_t rx_byte;
static char line_buf[RX_BUF_SIZE];
static volatile uint16_t line_idx = 0;
static volatile uint8_t line_ready = 0;

static void print_prompt(void) {
    printf("\r\n> ");
}
static void print_centi(int32_t v){
    printf("%s%ld.%02ld", v < 0 ? "-" : "", labs(v) / 100, labs(v) % 100);
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
    printf("  env         - θερμοκρασία/πίεση/υγρασία σε φυσικές μονάδες\r\n");
    printf("  imu         - επιτάχυνση/γυροσκόπιο/θερμοκρασία σε φυσικές μονάδες\r\n");
    printf("  mpucal      - βαθμονόμηση γυροσκοπίου (κράτα το board ακίνητο)\r\n");
    printf("  log start [ms] - ξεκινά την καταγραφή (default 1000 ms)\r\n");
    printf("  log stop       - σταματά την καταγραφή\r\n");
    printf("  log status     - κατάσταση της καταγραφής\r\n");
    printf("  log dump       - εκτυπώνει τα δεδομένα (CSV)\r\n");
    printf("  log clear      - σβήνει τα δεδομένα\r\n");
    printf("  rtos        - χρήση stack και heap του FreeRTOS\r\n");
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
            if (cli_task != NULL) {
                BaseType_t woken = pdFALSE;
                vTaskNotifyGiveFromISR(cli_task, &woken);
                portYIELD_FROM_ISR(woken);
            }
        } else if (line_idx < RX_BUF_SIZE - 1) {
            line_buf[line_idx++] = rx_byte;
            HAL_UART_Transmit(menu_huart, &rx_byte, 1, 10); // echo
        }
        HAL_UART_Receive_IT(menu_huart, &rx_byte, 1); // ξανά-άκουσε
    }
}
static void handle_log(const char *args)
{
    while (*args == ' ') {
        args++;
    }

    if (strncmp(args, "start", 5) == 0) {
        uint32_t period = (uint32_t)atoi(args + 5);  /* 0 if no number given */
        if (period == 0) {
            period = DATALOGGER_DEFAULT_MS;
        }
        DataLogger_Start(period);
        printf("\r\nΚαταγραφή ξεκίνησε, περίοδος %lu ms", DataLogger_Period());
    } else if (strcmp(args, "stop") == 0) {
        DataLogger_Stop();
        printf("\r\nΚαταγραφή σταμάτησε, %lu εγγραφές", DataLogger_Count());
    } else if (strcmp(args, "clear") == 0) {
        DataLogger_Clear();
        printf("\r\nΤα δεδομένα σβήστηκαν");
    } else if (strcmp(args, "dump") == 0) {
        DataLogger_Dump();
    } else if (strcmp(args, "status") == 0) {
    	printf("\r\nΚαταγραφή: %s, περίοδος %lu ms, εγγραφές %lu/%d, σφάλματα %lu, χαμένα δείγματα %lu",
    	       DataLogger_IsRunning() ? "ενεργή" : "ανενεργή",
    	       DataLogger_Period(), DataLogger_Count(), DATALOGGER_CAPACITY,
    	       DataLogger_ErrorCount(),
    	       DataLogger_DroppedCount() + SensorTask_DroppedCount());
    } else {
        printf("\r\nΧρήση: log start [ms] | stop | status | dump | clear");
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
    } else if (strcmp(cmd, "env") == 0) {
        BME280_Data d;
        if (BME280_Read(&d) == BME280_OK) {
            int32_t t = d.temp_centi;
            printf("\r\nT=%s%ld.%02ld C  P=%lu.%02lu hPa  H=%lu.%02lu %%RH",
                   t < 0 ? "-" : "", labs(t) / 100, labs(t) % 100,
                   d.press_pa / 100, d.press_pa % 100,
                   d.hum_centi / 100, d.hum_centi % 100);
        } else {
            printf("\r\nBME280 read error");
        }
    } else if (strcmp(cmd, "imu") == 0) {
        MPU6050_Data m;
        if (MPU6050_Read(&m) == MPU6050_OK) {
            printf("\r\nAccel [mg]:  x=%ld  y=%ld  z=%ld",
                   m.accel_mg_x, m.accel_mg_y, m.accel_mg_z);
            printf("\r\nGyro [dps]:  x="); print_centi(m.gyro_cdps_x);
            printf("  y="); print_centi(m.gyro_cdps_y);
            printf("  z="); print_centi(m.gyro_cdps_z);
            printf("\r\nTemp [C]:    "); print_centi(m.temp_centi);
        } else {
            printf("\r\nMPU6050 read error");
        }
    } else if (strcmp(cmd, "mpucal") == 0) {
        printf("\r\nΒαθμονόμηση... κράτα το board ακίνητο");
        if (MPU6050_CalibrateGyro(200) == MPU6050_OK) {
            printf("\r\nΤέλος βαθμονόμησης");
        } else {
            printf("\r\nMPU6050 read error");
        }
    } else if (strcmp(cmd, "log") == 0 || strncmp(cmd, "log ", 4) == 0) {
        handle_log(cmd + 3);
    } else if (strcmp(cmd, "rtos") == 0) {
        static char list[400];
        vTaskList(list);
        printf("\r\nTask        State Prio Stack Num\r\n%s", list);
        printf("Stack = ελάχιστος ελεύθερος χώρος (words)\r\n");
        printf("Heap: ελεύθερος %lu bytes, ελάχιστος ποτέ %lu bytes",
               (unsigned long)xPortGetFreeHeapSize(),
               (unsigned long)xPortGetMinimumEverFreeHeapSize());
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
static void cli_task_fn(void *argument)
{
    (void)argument;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   /* κοιμάται μέχρι να έρθει πλήρης γραμμή */
        CommandMenu_Process();
    }
}

void CommandMenu_Create(void)
{
    xTaskCreate(cli_task_fn, "CLI", STACK_CLI, NULL, PRIO_CLI, &cli_task);
}
