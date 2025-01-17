/*
 * Copyright (c) 2024 Evercars
 */

#include "spi_event.h"
#include "spi_module.h"
#include "ui_terminal_event.h"

/* Include the header files for SPI, GPIO and devicetree */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
 
#define ENABLE_TESTING
#define MODULE          spi
#define SPI_LOG_LEVEL   4
LOG_MODULE_REGISTER(MODULE, SPI_LOG_LEVEL);

/* Retrieve the API-device structure */
#define SPIOP	    SPI_WORD_SET(8) | SPI_TRANSFER_MSB
#define SPI_DEVICE  DT_NODELABEL(spi_master)

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

/* Function prototypes */
static int spi_init(void);
static int send_header(uint8_t endpoint, uint16_t req_len, uint16_t resp_len);
static int write_to_uart(void);
static int print_version(void);
static int spi_recovery(void);
static int spi_recovery_test(void);
static int spi_can_test(void);

static uint8_t calculate_checksum(const uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum;
}

static int send_header(uint8_t endpoint, uint16_t req_len, uint16_t resp_len)
{
    int err;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[REQ_HEADER_LEN] = {0};
    uint8_t rx_buffer[RESP_HEADER_LEN] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    // Set header
    tx_buffer[0] = SPI_SYNC_BYTE;
    tx_buffer[1] = endpoint;
    tx_buffer[2] = req_len & 0xFFU;
    tx_buffer[3] = (req_len >> 8) & 0xFFU;
    tx_buffer[4] = resp_len & 0xFFU;
    tx_buffer[5] = (resp_len >> 8) & 0xFFU;
    tx_buffer[6] = calculate_checksum(tx_buffer, 6);

    /* Call the transceive function */
    LOG_INF("--- Sending header (%d) ---", req_len);
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

    if (send_header(SPI_ENDPOINT_UART_WRITE, sizeof(test_string)+1, 0) != 0)
        return -1;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[sizeof(test_string) + 2];
    uint8_t rx_buffer[RESP_DATA_LEN] = {0};
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
    uint8_t tx_buffer[REQ_VERSION_LEN] = { 0x56, 0x45, 0x52, 0x53, 0x49, 0x4F, 0x4E }; // VERSION
    uint8_t rx_buffer[RESP_VERSION_LEN] = {0};
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

static int spi_recovery(void)
{
    /* Set the transmit and receive buffers */
    uint8_t rx_buffer[24] = {0}; // Incomplete read
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    spi_read(spi_dev, &spi_cfg, &rx_spi_buf_set);

    return 0;
}

#ifdef ENABLE_TESTING
static int spi_recovery_test(void)
{
    int err;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[REQ_VERSION_LEN] = { 0x56, 0x45, 0x52, 0x53, 0x49, 0x4F, 0x4E }; // VERSION
    uint8_t rx_buffer[RESP_VERSION_LEN - 5] = {0}; // Incomplete read
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    /* Call the transceive function */
    LOG_INF("--- Incomplete version read ---");
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

    // Read all data from SPI
    LOG_INF("--- Recovering SPI ---");
    spi_recovery();

    // Try to read the version
    LOG_INF("--- Trying to read the version again ---");
    print_version();

    return 0;
}

static int spi_can_test(void)
{
    int err;

    if (send_header(SPI_ENDPOINT_CAN_READ, 0, sizeof(CANPacket_t)) != 0)
        return -1;

    /* Set the transmit and receive buffers */
    uint8_t tx_buffer[sizeof(SPI_CHECKSUM_START)] = { SPI_CHECKSUM_START };
    uint8_t rx_buffer[RESP_CAN_READ_LEN] = {0};
    struct spi_buf tx_spi_buf			= {.buf = tx_buffer, .len = sizeof(tx_buffer)};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs 			= {.buf = rx_buffer, .len = sizeof(rx_buffer)};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    /* Call the transceive function */
    LOG_INF("--- Sending CAN Read ---");
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

    CANPacket_t *can_packet = (CANPacket_t *)&rx_buffer[3];
    LOG_INF("CAN Addr: %d", can_packet->addr);
    LOG_INF("CAN Bus: %d", can_packet->bus);
    LOG_INF("CAN Data Length: %d", can_packet->data_len_code);
    printk("CAN Data: ");
    for (int i = 0; i < can_packet->data_len_code; i++)
        printk("0x%X", can_packet->data[i]);
    printk("\n");

    return 0;
}
#else
static int spi_recovery_test(void)
{
    LOG_INF("SPI recovery test is disabled");
    return 0;
}
#endif

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

    switch (ev->type) {
        case SPI_COMM_VERSION:
            LOG_INF("Getting panda version");
            print_version();
            break;
        case SPI_COMM_CAN:
            LOG_INF("Testing SPI data transfer");
            spi_can_test();
            break;
        case SPI_COMM_HELLO:
            LOG_INF("Saying hello to Panda");
            write_to_uart();
            break;
        case SPI_COMM_RECOVERY:
            LOG_INF("Performing SPI recovery test");
            spi_recovery_test();
            break;
        default:
            break;
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