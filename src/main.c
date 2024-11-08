/*
* Copyright (c) 2012-2014 Wind River Systems, Inc.
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

// Exit codes
#define SUCCESS	0
#define FAILURE 1


#define LOOP_SLEEP_MS	1000
#define MY_SPI_SLAVE 	DT_NODELABEL(spi_test)
#define LED0_NODE 		DT_ALIAS(led0)
#define SPIOP			SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE

static const struct spi_config spi_cfg = {
	.operation = SPIOP,
	.frequency = 200000,
	.slave = 1,
};

static uint8_t tx_buffer[1];
static uint8_t rx_buffer[1];
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

static void spi_test_recv(void)
{
	int ret;

	struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer),
	};
	struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = sizeof(rx_buffer),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	ret = spi_read(spi_dev, &spi_cfg, &rx);
	if (ret < 0) {
		printk("SPI read error: %d\n", ret);
		return;
	}
	// Write the data back incremented by 1
	tx_buffer[0] = rx_buffer[0] + 1;
	ret = spi_write(spi_dev, &spi_cfg, &tx);
	if (ret != SUCCESS) {
		printk("SPI write error: %d\n", ret);
		return;
	}
	// Data received
	printk("-> RX: %d\n", rx_buffer[0]);
	printk("<- TX: %d\n", tx_buffer[0]);
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
		spi_test_recv();
		// k_sleep(K_MSEC(LOOP_SLEEP_MS));
	}

	return 0;
}
