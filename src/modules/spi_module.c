/*
 * Copyright (c) 2024 Evercars
 */

#include "spi_event.h"
#include "ui_terminal_event.h"

/* Include the header files for SPI, GPIO and devicetree */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
 
#define MODULE          spi
#define SPI_LOG_LEVEL   4
LOG_MODULE_REGISTER(MODULE, SPI_LOG_LEVEL);

#define DELAY_VALUES    1000

/* SPI Comm to Panda Defines */
#define VERSION_REQ_LEN     7
#define VERSION_RESP_LEN    25
#define HEADER_REQ_LEN      7
#define HEADER_RESP_LEN     1
#define DATA_RESP_LEN       4

/* Retrieve the API-device structure */
#define SPIOP	    SPI_WORD_SET(8) | SPI_TRANSFER_MSB
#define SPI_DEVICE  DT_NODELABEL(spi_master)

/* Panda protocol defines */
#define SPI_CHECKSUM_START  0xABU
#define SPI_SYNC_BYTE       0x5AU
#define SPI_HACK            0x79U
#define SPI_DACK            0x85U
#define SPI_NACK            0x1FU

#define SPI_ENDPOINT_CONTROL    0x00U
#define SPI_ENDPOINT_CAN_READ   0x01U
#define SPI_ENDPOINT_UART_WRITE 0x02U
#define SPI_ENDPOINT_CAN_WRITE  0x03U
#define SPI_ENDPOINT_TEST       0xABU

static const struct device *spi_dev;
static const struct spi_config spi_cfg = {
    .operation = SPIOP,
    .frequency = (KHZ(250)),
    .cs = {
        .gpio = GPIO_DT_SPEC_GET(SPI_DEVICE, cs_gpios),
        .delay = 0,
    },
};
static uint8_t test_string[] = "Hello, Panda!\n";

static uint8_t calculate_checksum(const uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum;
}

static int send_header(void)
{
    int err;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[HEADER_REQ_LEN] = {0};
    uint8_t rx_buffer[HEADER_RESP_LEN] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    // Set header
    int packet_len = sizeof(test_string) + 1;
    tx_buffer[0] = SPI_SYNC_BYTE;
    tx_buffer[1] = SPI_ENDPOINT_UART_WRITE;
    tx_buffer[2] = packet_len & 0xFFU;
    tx_buffer[3] = (packet_len >> 8) & 0xFFU;
    tx_buffer[6] = calculate_checksum(tx_buffer, 6);

    /* Call the transceive function */
    LOG_INF("--- Sending header (%d) ---", packet_len);
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


static int write_to_uart(void)
{
    int err;

    if (send_header() != 0)
        return -1;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[sizeof(test_string) + 2];
    uint8_t rx_buffer[DATA_RESP_LEN] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    // Set Data
    tx_buffer[0] = 0;
    memcpy(&tx_buffer[1], test_string, sizeof(test_string));
    tx_buffer[sizeof(test_string) + 1] = calculate_checksum(tx_buffer, sizeof(test_string) + 1);

    /* Call the transceive function */
    LOG_INF("--- Sending UART Data ---");
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

static int print_version(void)
{
    int err;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[VERSION_REQ_LEN] = { 0x56, 0x45, 0x52, 0x53, 0x49, 0x4F, 0x4E }; // VERSION
    uint8_t rx_buffer[VERSION_RESP_LEN] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    /* Call the transceive function */
    LOG_INF("--- Sending Version ---");
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

static int spi_init(void)
{
    /* Check if SPI and GPIO devices are ready */
    spi_dev = DEVICE_DT_GET(SPI_DEVICE);
    int err = device_is_ready(spi_dev);
    if (!err) {
        LOG_ERR("Error: SPI device is not ready, err: %d", err);
        return err;
    }
    LOG_INF("SPI initialized");

    return 0;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
    if (!is_spi_event(aeh))
        return false;

    struct spi_event *ev = cast_spi_event(aeh);

    if (ev->type == SPI_COMM_VERSION) {
        LOG_INF("Getting panda version");
        print_version();
    } else if (ev->type == SPI_COMM_CAN) {
        LOG_INF("Testing SPI data transfer");
    } else if (ev->type == SPI_COMM_HELLO) {
        LOG_INF("Saying hello to Panda");
        write_to_uart();
    }

    // Send UART event
    struct ui_terminal_event *ui_terminal_event = new_ui_terminal_event();
    __ASSERT(ui_terminal_event, "Not enough heap left to allocate event");
    ui_terminal_event->accept_input = true;
    APP_EVENT_SUBMIT(ui_terminal_event);

    return true;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, spi_event);

SYS_INIT(spi_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);