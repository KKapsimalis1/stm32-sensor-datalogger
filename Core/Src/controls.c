#include "controls.h"
#include "app_config.h"
#include "button.h"
#include "data_logger.h"
#include "display_task.h"
#include "mpu6050.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define MESSAGE_MS  1500

static TaskHandle_t task_handle;
static uint8_t      last_running;

static void set_led(uint8_t on)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/*
 * Handles button events. Feedback goes to the OLED only: the UART belongs to the CLI task,
 * so no other task prints to it.
 */
static void process_button(void)
{
    switch (Button_GetEvent()) {

    case BUTTON_SHORT:
        if (DataLogger_IsRunning()) {
            char line[22];
            DataLogger_Stop();
            snprintf(line, sizeof(line), "%lu records", (unsigned long)DataLogger_Count());
            DisplayTask_ShowMessage("Logging STOPPED", line, MESSAGE_MS);
        } else {
            DataLogger_Clear();
            DataLogger_Start(DATALOGGER_DEFAULT_MS);
            DisplayTask_ShowMessage("Logging STARTED", "buffer cleared", MESSAGE_MS);
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
            } else {
                DisplayTask_ShowMessage("Calibration error", NULL, MESSAGE_MS);
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

static void controls_task(void *argument)
{
    (void)argument;

    for (;;) {
        /*
         * Sleep until the button interrupt wakes us. The timeout makes the task check the
         * button every 50 ms anyway, which is how a long press is detected while the button
         * is still held down (no edge arrives at that moment).
         */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        process_button();
    }
}

void Controls_Init(void)
{
    last_running = DataLogger_IsRunning();
    set_led(last_running);
}

void Controls_Create(void)
{
    xTaskCreate(controls_task, "Controls", STACK_CONTROLS, NULL, PRIO_CONTROLS, &task_handle);
}

void Controls_NotifyFromISR(void)
{
    if (task_handle != NULL) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(task_handle, &woken);
        portYIELD_FROM_ISR(woken);
    }
}
