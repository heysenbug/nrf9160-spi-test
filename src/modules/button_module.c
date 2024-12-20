/*
 * Copyright (c) 2024 Evercars
 */

#include "spi_event.h"

#include <dk_buttons_and_leds.h>
#include <zephyr/logging/log.h>

#define MODULE button
LOG_MODULE_REGISTER(MODULE);

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
    LOG_INF("Button pressed");
    if (has_changed & button_states & DK_BTN1_MSK) {

        LOG_INF("Button 1 pressed");
        struct spi_event *spi_module_event = new_spi_event();

        __ASSERT(spi_module_event, "Not enough heap left to allocate event");

        spi_module_event->type = SPI_COMM_VERSION;

        APP_EVENT_SUBMIT(spi_module_event);
    }

#if defined(CONFIG_BOARD_NRF9160DK_NRF9160_NS) || defined(CONFIG_BOARD_NRF9161DK_NRF9161_NS)
    if (has_changed & button_states & DK_BTN2_MSK) {

        LOG_INF("Button 2 pressed");
        struct spi_event *spi_module_event = new_spi_event();

        __ASSERT(spi_module_event, "Not enough heap left to allocate event");

        spi_module_event->type = SPI_COMM_VERSION;

        APP_EVENT_SUBMIT(spi_module_event);
    }
#endif
}

static int buttons_init(void)
{
    int err = dk_buttons_init(button_handler);
    if (err) {
        LOG_ERR("dk_buttons_init, error: %d", err);
        return err;
    }
    LOG_INF("Button initialized");

    return 0;
}

SYS_INIT(buttons_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);