#include "display_task.h"
#include "app_config.h"
#include "sensor_task.h"
#include "task.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>

#define DISPLAY_PERIOD_MS  1000
#define MESSAGE_QUEUE_LEN  4

typedef struct {
    char     line1[22];
    char     line2[22];
    uint32_t duration_ms;
} DisplayMessage;

static uint8_t       display_ready;
static QueueHandle_t message_queue;

/* Writes a value given in 0.01 units with one decimal, e.g. -123 -> "-1.2". */
static void write_deci(int32_t value_centi)
{
    int32_t t = value_centi / 10;  /* tenths, truncated toward zero */
    SSD1306_Printf("%s%ld.%ld", t < 0 ? "-" : "", labs(t) / 10, labs(t) % 10);
}

static void draw_measurements(void)
{
    SensorSample s;

    SSD1306_Clear();

    if (!SensorTask_GetLatest(&s)) {
        SSD1306_SetCursor(0, 24);
        SSD1306_WriteString("Waiting for sensors");
        SSD1306_Update();
        return;
    }

    /* Environment: 3 text rows */
    if (s.env_ok) {
        int32_t t = s.env.temp_centi;
        SSD1306_SetCursor(0, 0);
        SSD1306_Printf("T: %s%ld.%02ld C", t < 0 ? "-" : "", labs(t) / 100, labs(t) % 100);
        SSD1306_SetCursor(0, 8);
        SSD1306_Printf("P: %lu.%02lu hPa", s.env.press_pa / 100, s.env.press_pa % 100);
        SSD1306_SetCursor(0, 16);
        SSD1306_Printf("H: %lu.%02lu %%RH", s.env.hum_centi / 100, s.env.hum_centi % 100);
    } else {
        SSD1306_SetCursor(0, 0);
        SSD1306_WriteString("BME280: read error");
    }

    /* Separator line */
    for (uint8_t x = 0; x < SSD1306_WIDTH; x++) {
        SSD1306_DrawPixel(x, 27, 1);
    }

    /* Motion: accelerometer and gyroscope */
    if (s.imu_ok) {
        SSD1306_SetCursor(0, 32);
        SSD1306_WriteString("Accel [mg]");
        SSD1306_SetCursor(0, 40);
        SSD1306_Printf("%ld %ld %ld", s.imu.accel_mg_x, s.imu.accel_mg_y, s.imu.accel_mg_z);

        SSD1306_SetCursor(0, 48);
        SSD1306_WriteString("Gyro [dps]");
        SSD1306_SetCursor(0, 56);
        write_deci(s.imu.gyro_cdps_x);
        SSD1306_WriteChar(' ');
        write_deci(s.imu.gyro_cdps_y);
        SSD1306_WriteChar(' ');
        write_deci(s.imu.gyro_cdps_z);
    } else {
        SSD1306_SetCursor(0, 32);
        SSD1306_WriteString("MPU6050: read error");
    }

    SSD1306_Update();
}

static void draw_message(const DisplayMessage *m)
{
    SSD1306_Clear();
    SSD1306_SetCursor(0, 24);
    SSD1306_WriteString(m->line1);
    if (m->line2[0] != '\0') {
        SSD1306_SetCursor(0, 36);
        SSD1306_WriteString(m->line2);
    }
    SSD1306_Update();
}

static void display_task(void *argument)
{
    (void)argument;
    TickType_t next_refresh  = xTaskGetTickCount();
    TickType_t message_until = 0;
    uint8_t    message_active = 0;

    for (;;) {
        /* Sleep until the next event: a new message, the end of a message, or the next refresh. */
        TickType_t now = xTaskGetTickCount();
        int32_t remaining = message_active ? (int32_t)(message_until - now)
                                           : (int32_t)(next_refresh - now);
        if (remaining < 0) {
            remaining = 0;
        }

        DisplayMessage m;
        if (xQueueReceive(message_queue, &m, (TickType_t)remaining) == pdTRUE) {
            draw_message(&m);
            message_until = xTaskGetTickCount() + pdMS_TO_TICKS(m.duration_ms);
            message_active = 1;
            continue;
        }

        now = xTaskGetTickCount();
        if (message_active) {
            if ((int32_t)(now - message_until) < 0) {
                continue;               /* the message is still being shown */
            }
            message_active = 0;
            next_refresh = now;         /* redraw the measurements right away */
        }
        if ((int32_t)(now - next_refresh) >= 0) {
            draw_measurements();
            next_refresh = xTaskGetTickCount() + pdMS_TO_TICKS(DISPLAY_PERIOD_MS);
        }
    }
}

SSD1306_Status DisplayTask_Init(I2C_HandleTypeDef *hi2c)
{
    /* No FreeRTOS calls here: this runs before the scheduler starts. */
    SSD1306_Status st = SSD1306_Init(hi2c);
    display_ready = (st == SSD1306_OK);
    return st;
}

void DisplayTask_Create(void)
{
    if (display_ready) {
        message_queue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(DisplayMessage));
        xTaskCreate(display_task, "Display", STACK_DISPLAY, NULL, PRIO_DISPLAY, NULL);
    }
}

void DisplayTask_ShowMessage(const char *line1, const char *line2, uint32_t duration_ms)
{
    if (!display_ready || message_queue == NULL) {
        return;
    }

    DisplayMessage m;
    memset(&m, 0, sizeof(m));
    strncpy(m.line1, line1, sizeof(m.line1) - 1);
    if (line2 != NULL) {
        strncpy(m.line2, line2, sizeof(m.line2) - 1);
    }
    m.duration_ms = duration_ms;

    xQueueSend(message_queue, &m, 0);   /* if the queue is full, the message is dropped */
}
