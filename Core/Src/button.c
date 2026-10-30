#include "button.h"
#include "controls.h"
#include "FreeRTOS.h"
#include "task.h"

/* State shared between the EXTI interrupt and the controls task. */
static volatile uint8_t  pressed;           /* 1 while the button is held down */
static volatile uint32_t press_start_ms;
static volatile uint8_t  release_pending;   /* a release has not been consumed yet */
static volatile uint32_t press_duration_ms;
static volatile uint32_t last_edge_ms;

/* Used only by the controls task. */
static uint8_t long_reported;

/* Called by the HAL from EXTI15_10_IRQHandler on every edge of PC13. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != B1_Pin) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((now - last_edge_ms) < BUTTON_DEBOUNCE_MS) {
        return;  /* contact bounce */
    }
    last_edge_ms = now;

    /* Read the pin instead of assuming the edge direction: the button is active low. */
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {
        pressed = 1;
        press_start_ms = now;
    } else if (pressed) {
        pressed = 0;
        press_duration_ms = now - press_start_ms;
        release_pending = 1;
    }

    Controls_NotifyFromISR();  /* wake the controls task right away */
}

ButtonEvent Button_GetEvent(void)
{
    uint32_t now = HAL_GetTick();

    /* Take a consistent snapshot of the state shared with the interrupt. */
    taskENTER_CRITICAL();
    uint8_t  is_pressed = pressed;
    uint32_t start      = press_start_ms;
    uint8_t  released   = release_pending;
    uint32_t duration   = press_duration_ms;
    release_pending = 0;
    taskEXIT_CRITICAL();

    /* Long press: reported as soon as the hold time is reached, while the button is still down. */
    if (is_pressed && !long_reported && (now - start) >= BUTTON_LONG_PRESS_MS) {
        long_reported = 1;
        return BUTTON_LONG;
    }

    if (released) {
        if (long_reported) {
            long_reported = 0;   /* the long press was already reported */
            return BUTTON_NONE;
        }
        /* The main loop may have been busy and missed the "held" moment. */
        return (duration >= BUTTON_LONG_PRESS_MS) ? BUTTON_LONG : BUTTON_SHORT;
    }

    return BUTTON_NONE;
}
