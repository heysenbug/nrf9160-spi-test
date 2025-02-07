/*
 * Copyright (c) 2024 Evercars
 */
#include "spi_event.h"
#include "ui_terminal_event.h"

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
/* Include the header file of the UART driver in main.c */
#include <zephyr/drivers/uart.h>

#define MODULE uart
#define UART_LOG_LEVEL   4
LOG_MODULE_REGISTER(MODULE, UART_LOG_LEVEL);

/* Define the size of the receive buffer */
#define RECEIVE_BUFF_SIZE 10
/* Define the receiving timeout period */
#define RECEIVE_TIMEOUT 100
/* One entire line */
#define LINE_BUFF_SIZE 64

/* Get the device pointer of the UART hardware */
const struct device *uart;

/* Define the buffers */
static uint8_t rx_buf[RECEIVE_BUFF_SIZE] = {0};
static uint8_t line_buf[LINE_BUFF_SIZE] = {0};
static size_t line_buf_pos = 0;


static inline int enable_uart(void)
{
	return uart_rx_enable(uart, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
}

static inline int disable_uart(void)
{
	return uart_rx_disable(uart);
}

static void print_help(void)
{
    printk("UART Module\n");
    printk("Commands:\n");
    printk("\thelp - Print this help\n");
    printk("\tversion - Print Panda version\n");
    printk("\ttest - Test SPI data transfer\n");
    printk("\thello - Send a hello to Panda\n ");
    printk("\trtest - Perform a SPI recovery test\n ");
    printk("\tvin <req> <resp> - Perform a VIN request\n");
}

static bool parse_command(void)
{
	if (strncmp(line_buf, "help", 4) == 0) {
		print_help();
		return false;
	} else if (strncmp(line_buf, "version", 7) == 0) {
		struct spi_event *se = new_spi_event();
		__ASSERT(se, "Not enough heap left to allocate event");
		se->type = SPI_COMM_VERSION;
		APP_EVENT_SUBMIT(se);
		return true;
	} else if (strncmp(line_buf, "test", 4) == 0) {
		struct spi_event *se = new_spi_event();
		__ASSERT(se, "Not enough heap left to allocate event");
		se->type = SPI_COMM_CAN;
		APP_EVENT_SUBMIT(se);
		return true;
	} else if (strncmp(line_buf, "hello", 5) == 0) {
		struct spi_event *se = new_spi_event();
		__ASSERT(se, "Not enough heap left to allocate event");
		se->type = SPI_COMM_HELLO;
		APP_EVENT_SUBMIT(se);
		return true;
	} else if (strncmp(line_buf, "rtest", 5) == 0) {
		struct spi_event *se = new_spi_event();
		__ASSERT(se, "Not enough heap left to allocate event");
		se->type = SPI_COMM_RECOVERY;
		APP_EVENT_SUBMIT(se);
		return true;
	} else if (strncmp(line_buf, "vin", 3) == 0) {
		uint8_t *end;
		struct spi_event *se = new_spi_event();
		__ASSERT(se, "Not enough heap left to allocate event");
		se->type = SPI_COMM_CAN;
		se->address = strtol(&line_buf[4], &end, 16);
		se->response = strtol(end, NULL, 16);
		APP_EVENT_SUBMIT(se);
		return true;
	} else {
		printk("Unknown command\n> ");
	}	

	return false;
}

/* Define the callback function for UART */
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {

	case UART_RX_RDY:
		if ((evt->data.rx.len) <= 0)
            return;
        uint8_t byte = evt->data.rx.buf[evt->data.rx.offset];
        if (byte == '\n' || byte == '\r') {
            // Reset the line buffer position
            line_buf[line_buf_pos++] = '\n';
            line_buf[line_buf_pos++] = '\r';
            line_buf[line_buf_pos++] = '\0';
            line_buf_pos = 0;

			// Parse the command
			disable_uart();
			if (!parse_command())
				printk("\n> ");
        } else {
            // Store the received byte in the line buffer
            if (line_buf_pos < LINE_BUFF_SIZE - 1)
                line_buf[line_buf_pos++] = byte;
        }
		break;
	case UART_RX_DISABLED:
		uart_rx_enable(dev, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
		break;

	default:
		break;
	}
}

static int uart_init(void)
{
    uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
	/* Verify that the UART device is ready */
	if (!device_is_ready(uart)) {
		LOG_ERR("UART device not ready");
		return 1;
	}

	/* Register the UART callback function */
	if (uart_callback_set(uart, uart_cb, NULL)) {
        LOG_ERR("Failed to set UART callback");
		return 1;
	}
	/* Start receiving by calling uart_rx_enable() and pass it the address of the
	 * receive  buffer */
	if (enable_uart()) {
        LOG_ERR("Failed to enable UART RX");
		return 1;
	}

	print_help();

	return 0;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
    if (!is_ui_terminal_event(aeh))
        return false;

    struct ui_terminal_event *ev = cast_ui_terminal_event(aeh);

    if (ev->accept_input) {
		printk("\n> ");
	}

    return true;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, ui_terminal_event);

SYS_INIT(uart_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
