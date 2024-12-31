/*
 * Copyright (c) 2024 Evercars
 */
#include "spi_event.h"
#include "ui_terminal_event.h"

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
#define LINE_BUFF_SIZE 32

/* Get the device pointer of the UART hardware */
const struct device *uart;

/* Define the buffers */
static uint8_t rx_buf[RECEIVE_BUFF_SIZE] = {0};
static uint8_t line_buf[LINE_BUFF_SIZE] = {0};
static size_t line_buf_pos = 0;
static bool self_disable = false;

/* Function prototypes */
static void print_help(void);
static void enable_uart(void);
static void disable_uart(void);
static bool parse_command(void);
static int uart_init(void);
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data);

static void enable_uart(void)
{
	if (self_disable) {
		self_disable = false;
		printk("> ");
	}
	uart_rx_enable(uart, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
}

static void disable_uart(void)
{
	self_disable = true;
	uart_rx_disable(uart);
}

static bool parse_command(void)
{
	if (strncmp(line_buf, "help", 4) == 0) {
		print_help();
		return false;
	} else if (strncmp(line_buf, "version", 7) == 0) {
		struct spi_event *spi_module_event = new_spi_event();
		__ASSERT(spi_module_event, "Not enough heap left to allocate event");
		spi_module_event->type = SPI_COMM_VERSION;
		APP_EVENT_SUBMIT(spi_module_event);
		return true;
	} else if (strncmp(line_buf, "test", 4) == 0) {
		struct spi_event *spi_module_event = new_spi_event();
		__ASSERT(spi_module_event, "Not enough heap left to allocate event");
		spi_module_event->type = SPI_COMM_CAN;
		APP_EVENT_SUBMIT(spi_module_event);
		return true;
	} else if (strncmp(line_buf, "hello", 5) == 0) {
		struct spi_event *spi_module_event = new_spi_event();
		__ASSERT(spi_module_event, "Not enough heap left to allocate event");
		spi_module_event->type = SPI_COMM_HELLO;
		APP_EVENT_SUBMIT(spi_module_event);
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
				enable_uart();
        } else {
            // Store the received byte in the line buffer
            if (line_buf_pos < LINE_BUFF_SIZE - 1)
                line_buf[line_buf_pos++] = byte;
        }
		break;
	case UART_RX_DISABLED:
		if (self_disable)
			return;
		uart_rx_enable(dev, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
		break;

	default:
		break;
	}
}

static void print_help(void)
{
    printk("UART Module\n");
    printk("Commands:\n");
    printk("\thelp - Print this help\n");
    printk("\tversion - Print Panda version\n");
    printk("\ttest - Test SPI data transfer\n");
    printk("\thello - Send a hello to Panda\n> ");
}

static int uart_init(void)
{
	int ret;

    uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
	/* Verify that the UART device is ready */
	if (!device_is_ready(uart)) {
		LOG_ERR("UART device not ready");
		return 1;
	}

	/* Register the UART callback function */
	ret = uart_callback_set(uart, uart_cb, NULL);
	if (ret) {
        LOG_ERR("Failed to set UART callback");
		return 1;
	}
	/* Start receiving by calling uart_rx_enable() and pass it the address of the
	 * receive  buffer */
	ret = uart_rx_enable(uart, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
	if (ret) {
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

    if (ev->accept_input)
		enable_uart();

    return true;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, ui_terminal_event);

SYS_INIT(uart_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
