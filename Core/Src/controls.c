#include "controls.h"
#include "button.h"
#include "data_logger.h"
#include "display_task.h"
#include "mpu6050.h"
#include <stdio.h>

#define MESSAGE_MS  1500

static uint8_t last_running;

static void set_led(uint8_t on)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Controls_Init(void)
{
    last_running = DataLogger_IsRunning();
    set_led(last_running);
}

void Controls_Process(void)
{
    switch (Button_GetEvent()) {

    case BUTTON_SHORT:
        if (DataLogger_IsRunning()) {
            char line[22];
            DataLogger_Stop();
            snprintf(line, sizeof(line), "%lu records", DataLogger_Count());
            DisplayTask_ShowMessage("Logging STOPPED", line, MESSAGE_MS);
            printf("\r\n[button] logging stopped, %lu records", DataLogger_Count());
        } else {
            DataLogger_Clear();
            DataLogger_Start(DATALOGGER_DEFAULT_MS);
            DisplayTask_ShowMessage("Logging STARTED", "buffer cleared", MESSAGE_MS);
            printf("\r\n[button] logging started, period %lu ms", DataLogger_Period());
        }
        break;

    case BUTTON_LONG:
        if (DataLogger_IsRunning()) {
            /* Calibrating during a recording would leave a gap and change the offset mid-log. */
            DisplayTask_ShowMessage("Stop the log first", "(short press)", MESSAGE_MS);
        } else {
            DisplayTask_ShowMessage("Calibrating gyro", "keep board still", MESSAGE_MS);
            if (MPU6050_CalibrateGyro(200) == MPU6050_OK) {
                DisplayTask_ShowMessage("Gyro calibrated", NULL, MESSAGE_MS);
                printf("\r\n[button] gyro calibrated");
            } else {
                DisplayTask_ShowMessage("Calibration error", NULL, MESSAGE_MS);
                printf("\r\n[button] gyro calibration failed");
            }
        }
        break;

    default:
        break;
    }

    /* Keep LD2 in sync with the logger, whatever started it (button or UART command). */
    uint8_t running = DataLogger_IsRunning();
    if (running != last_running) {
        last_running = running;
        set_led(running);
    }
}
