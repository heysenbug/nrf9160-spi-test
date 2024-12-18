/*
 * Copyright (c) 2024 Evercars
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Include the header files for SPI, GPIO and devicetree */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

/* CAF Application Event Manager Framework */
#include <app_event_manager.h>

#define MODULE main
#include <caf/events/module_state_event.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

#define DELAY_VALUES    1000
#define LED0	        13

const struct gpio_dt_spec ledspec = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);

/* Retrieve the API-device structure */
#define SPIOP	    SPI_WORD_SET(8) | SPI_TRANSFER_MSB
#define SPI_DEVICE  DT_NODELABEL(spi_master)

static const struct device *spi_dev = DEVICE_DT_GET(SPI_DEVICE);
static const struct spi_config spi_cfg = {
    .operation = SPIOP,
    .frequency = (KHZ(250)),
    .cs = {
        .gpio = GPIO_DT_SPEC_GET(SPI_DEVICE, cs_gpios),
        .delay = 0,
    },
};


int panda_read_version(void)
{
    int err;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[7] = { 0x56, 0x45, 0x52, 0x53, 0x49, 0x4F, 0x4E }; // VERSION
    // uint8_t tx_buffer[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 }; // VERSION
    uint8_t rx_buffer[24] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    /* Call the transceive function */
    LOG_INF("Sending %d bytes and receiving %d", sizeof(tx_buffer), sizeof(rx_buffer));
    err = spi_write(spi_dev, &spi_cfg, &tx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_write() failed, err: %d", err);
        return err;
    }
    err = spi_read(spi_dev, &spi_cfg, &rx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_read() failed, err: %d", err);
        return err;
    }

    printk("(%d): ", err);
    for (int i = 0; i < sizeof(rx_buffer); i++)
        printk("%X ", rx_buffer[i]);
    printk("\n");

    return 0;
}

void print_banner(void)
{
    LOG_INF("***********************");
    LOG_INF("      PANDA SPI");
    LOG_INF("***********************");
}

int main(void)
{
    int err;

    if (app_event_manager_init()) {
        LOG_ERR("Application Event Manager not initialized");
    } else {
        module_set_state(MODULE_STATE_READY);
    }
    
    err = gpio_is_ready_dt(&ledspec);
    if (!err) {
        LOG_ERR("Error: GPIO device is not ready, err: %d", err);
        return 0;
    }

    print_banner();
    /* Check if SPI and GPIO devices are ready */
    err = device_is_ready(spi_dev);
    if (!err) {
        LOG_ERR("Error: SPI device is not ready, err: %d", err);
        return 0;
    }

    gpio_pin_configure_dt(&ledspec, GPIO_OUTPUT_ACTIVE);
    
    // Get panda version
    LOG_INF("Reading panda version...");
    panda_read_version();
    
    LOG_INF("We're done.");

    while(1){
        /* Continuously read sensor samples and toggle led */
        gpio_pin_toggle_dt(&ledspec);
        k_msleep(DELAY_VALUES);
    }

    return 0;
}
