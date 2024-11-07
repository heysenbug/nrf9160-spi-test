/*
* Copyright (c) 2012-2014 Wind River Systems, Inc.
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/spi.h>


#define MY_SPI_MASTER DT_NODELABEL(spi_test)

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 200000,
	.slave = 0,
};

struct device * spi_dev;

static void spi_init(void)
{
	printk("Initializng spi device\n");
	spi_dev = DEVICE_DT_GET(MY_SPI_MASTER);

	if (spi_dev == NULL) {
		printk("Could not get %s device\n", spi_dev);
		return;
	}
}

void spi_test_send(void)
{
	int err;
	static uint8_t tx_buffer[1];
	static uint8_t rx_buffer[1];

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

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		printk("TX sent: %x\n", tx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[0]);
		tx_buffer[0]++;
	}	
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
	spi_init();

	printk("Executing main loop\n");
	while (1) {
		spi_test_send();
		k_sleep(K_MSEC(1000));
		printk(".");
	}

	return 0;
}
