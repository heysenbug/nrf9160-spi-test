/*
* Copyright (c) 2012-2014 Wind River Systems, Inc.
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/spi.h>


// Exit codes
#define SUCCESS	0
#define FAILURE 1


#define LOOP_SLEEP_MS   1000
#define MY_SPI_SLAVE    DT_NODELABEL(spi_test)
#define LED0_NODE       DT_ALIAS(led0)
#define SPIOP           SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER
#define SPI_DEVICE      DT_NODELABEL(spi_test)

// Panda SPI defines
#define VERSION_DATA_LEN    24

static const struct spi_config spi_cfg = {
    .operation = SPIOP,
    .frequency = KHZ(200),
    .cs = {
        .gpio = GPIO_DT_SPEC_GET(SPI_DEVICE, cs_gpios),
        .delay = 1,
    },
};

static uint8_t tx_buffer[7] = "VERSION";
static uint8_t rx_buffer[VERSION_DATA_LEN];
static const struct device *spi_dev = DEVICE_DT_GET(MY_SPI_SLAVE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static int spi_init(void)
{
    printk("Initializng spi device\n");

    if (spi_dev == NULL) {
        printk("Could not get %s device\n", spi_dev);
        return FAILURE;
    }

    if (!device_is_ready(spi_dev)) {
        printk("SPI device is not ready\n");
        return FAILURE;
    }

    return SUCCESS;
}

static int led_init(void)
{
    printk("Initializing LED\n");
    if (!gpio_is_ready_dt(&led)) {
        printk("GPIO isn't ready\n");
        return FAILURE;
    }

    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
        printk("Unable to configure LED\n");
        return FAILURE;
    }

    return SUCCESS;
}

void spi_test_send(void)
{
    int err;

    const struct spi_buf tx_buf = {
        .buf = tx_buffer,
        .len = sizeof(tx_buffer)
    };
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };

    struct spi_buf rx_buf = {
        .buf = rx_buffer,
        .len = sizeof(rx_buffer),
    };
    const struct spi_buf_set rx = {
        .buffers = &rx_buf,
        .count = 1
    };

    memset(rx_buffer, 0, sizeof(rx_buffer));
    err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
    if (err < 0) {
        printk("SPI error: %d\n", err);
        return;
    }

    // Received data
    printk("(%d) ", err);
    for (int i = 0; i < VERSION_DATA_LEN; i++)
        printk("0x%X ", rx_buffer[i]);
    printk("\n");
}

static void print_banner(void)
{
    printk("******************\n");
    printk("     SPI TEST\n");
    printk("******************\n\n");
}

int main(void)
{
    print_banner();
    if (spi_init() == FAILURE)
        return 0;

    if (led_init() == FAILURE)
        return 0;

    printk("Executing main loop\n");
    while (1) {
        gpio_pin_toggle_dt(&led);
        spi_test_send();
        k_sleep(K_MSEC(LOOP_SLEEP_MS));
    }

    return 0;
}
