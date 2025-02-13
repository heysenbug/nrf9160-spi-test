/*
 * Copyright (c) 2024 Evercars
 */

#include "spi_event.h"
#include "spi_module.h"
#ifdef CONFIG_TERMINAL_UI
#include "ui_terminal_event.h"
#endif

/* Include the header files for SPI, GPIO and devicetree */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
 
#define ENABLE_TESTING
#define MODULE          spi
#define SPI_LOG_LEVEL   LOG_LEVEL_DBG
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

static uint8_t send_buf[SPI_SEND_BUF_SIZE];
static uint8_t recv_buf[SPI_RECV_BUF_SIZE];
static const uint8_t test_string[] = "Hello, Panda!\n";
static const uint8_t version_string[REQ_VERSION_LEN] = "VERSION";

/* CAN related */
static uint8_t OBDII_VIN_REQUEST[3] = { 0x02, 0x09, 0x02 };
static uint8_t UDS_VIN_REQUEST[4] = { 0x03, 0x22, 0xF1, 0x90 };
static const unsigned char dlc_to_len[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};

/* Function prototypes */
static int spi_init(void);
static int send_header(uint8_t endpoint, uint16_t req_len, uint16_t resp_len);
static int write_to_uart(void);
static int print_version(void);
static int spi_recovery(void);
static int spi_recovery_test(void);
static int can_write_read(struct spi_event *ev);

static uint8_t calculate_checksum(const uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum;
}

static int _send(const uint16_t tx_len)
{
    int err;
    struct spi_buf tx_spi_buf			= {.buf = send_buf, .len = tx_len};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};

    err = spi_write(spi_dev, &spi_cfg, &tx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_write() failed, err: %d", err);
        return err;
    }

    return 0;
}

static int _receive(const uint16_t rx_len)
{
    int err;
    struct spi_buf rx_spi_bufs 			= {.buf = recv_buf, .len = rx_len};
    struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    err = spi_read(spi_dev, &spi_cfg, &rx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_read() failed, err: %d", err);
        return err;
    }

    return 0;
}

static int wait_for_ack(uint8_t ack_type, uint16_t wait_cycles)
{
    // fixme: Maybe implement this, I don't know yet
    // struct spi_buf rx_spi_bufs 			= {.buf = recv_buf, .len = RESP_HEADER_LEN};
    // struct spi_buf_set rx_spi_buf_set	= {.buffers = &rx_spi_bufs, .count = 1};

    return 0;
}

static int send_header(uint8_t endpoint, uint16_t req_len, uint16_t resp_len)
{
    /* Set the transmit and receive buffers */
    memset(send_buf, 0, REQ_HEADER_LEN);
    memset(recv_buf, 0, RESP_HEADER_LEN);
    // Set header
    send_buf[0] = SPI_SYNC_BYTE;
    send_buf[1] = endpoint;
    send_buf[2] = req_len & 0xFFU;
    send_buf[3] = (req_len >> 8) & 0xFFU;
    send_buf[4] = resp_len & 0xFFU;
    send_buf[5] = (resp_len >> 8) & 0xFFU;
    send_buf[6] = calculate_checksum(send_buf, 6);

    LOG_HEXDUMP_DBG(send_buf, 7, "HEADER: ");
    if (_send(REQ_HEADER_LEN) != 0)
        return -1;
    if (_receive(RESP_HEADER_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_HEADER_LEN, "HEADER RESP: ");

    return 0;
}

static int set_safety_mode(uint16_t mode)
{
    ControlPacket_t *control;
    const int send_len = sizeof(ControlPacket_t) + 1; // 1 for checksum

    if (send_header(SPI_ENDPOINT_UART_WRITE, send_len - 1, 0) != 0)
        return -1;

    /* Set the transmit and receive buffers */
    memset(send_buf, 0, send_len);
    memset(recv_buf, 0, RESP_DATA_LEN);
    // Set Data
    control = (ControlPacket_t *)send_buf;
    control->request = 0xDC;
    control->param1 = mode;
    send_buf[sizeof(ControlPacket_t)] = calculate_checksum(send_buf, sizeof(ControlPacket_t));

    /* Call the transceive function */
    LOG_HEXDUMP_DBG(send_buf, send_len, "CONTROL SEND: ");
    if (_send(send_len) != 0)
        return -1;
    if (_receive(RESP_DATA_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_DATA_LEN, "CONTROL RESP: ");

    return 0;
}

static int write_to_uart(void)
{
    if (send_header(SPI_ENDPOINT_UART_WRITE, sizeof(test_string)+1, 0) != 0)
        return -1;

    const int send_len = sizeof(test_string) + 2; // 1 for checksum and 1 for header
    /* Set the transmit and receive buffers */
    memset(send_buf, 0, send_len); // 1 for checksum and 1 for header
    memset(recv_buf, 0, RESP_DATA_LEN);
    // Set Data
    send_buf[0] = 0;
    memcpy(&send_buf[1], test_string, sizeof(test_string));
    send_buf[sizeof(test_string) + 1] = calculate_checksum(send_buf, sizeof(test_string) + 1);

    /* Call the transceive function */
    LOG_HEXDUMP_DBG(send_buf, send_len, "UART SEND: ");
    if (_send(send_len) != 0)
        return -1;
    if (_receive(RESP_DATA_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_DATA_LEN, "UART RESP: ");

    return 0;
}

static int print_version(void)
{
    /* Set the transmit and receive buffers */
    memcpy(send_buf, version_string, REQ_VERSION_LEN);
    memset(recv_buf, 0, RESP_VERSION_LEN);
    /* Call the transceive function */
    LOG_HEXDUMP_DBG(send_buf, REQ_VERSION_LEN, "VERSION SEND: ");
    if (_send(REQ_VERSION_LEN) != 0)
        return -1;
    if (_receive(RESP_VERSION_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_VERSION_LEN, "VERSION RESP: ");

    return 0;
}

static int spi_recovery(void)
{
    /* Set the transmit and receive buffers */
    if (_receive(24) != 0)
        return -1;
    return 0;
}

static int can_write(uint32_t addr)
{ 
    CANPacket_t *cpack;
    const int cpack_len = sizeof(CANPacket_t) - CANPACKET_DATA_SIZE_MAX + 8;

    /* Set the transmit and receive buffers */
    if (send_header(SPI_ENDPOINT_CAN_WRITE, cpack_len, 0) != 0)
        return -1;
    memset(send_buf, 0, cpack_len + 1);
    memset(recv_buf, 0, RESP_DATA_LEN);

    cpack = (CANPacket_t *)send_buf;
    cpack->addr = addr;
    cpack->data_len_code = 8;
    memcpy(cpack->data, UDS_VIN_REQUEST, sizeof(UDS_VIN_REQUEST));
    // 29-bit identifier
    if (addr >= 0x800)
        cpack->extended = 1;
    send_buf[cpack_len] = calculate_checksum(send_buf, cpack_len);

    LOG_HEXDUMP_DBG(send_buf, cpack_len + 1, "CAN_WRITE - TX: ");
    if (_send(cpack_len + 1) != 0)
        return -1;
    if (_receive(RESP_DATA_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_DATA_LEN, "CAN_WRITE - RX: ");
    if (recv_buf[0] != SPI_DACK) {
        LOG_ERR("Can data write couldn't be acknowledged");
        return -1;
    }

    return 0;
}

static int can_send_flow_control(uint32_t addr)
{
    CANPacket_t *cpack;
    const int cpack_len = sizeof(CANPacket_t) - CANPACKET_DATA_SIZE_MAX + 8;

    /* Set the transmit and receive buffers */
    // const int can_packet_len = sizeof(CANPacket_t) - CANPACKET_DATA_SIZE_MAX + 8;
    if (send_header(SPI_ENDPOINT_CAN_WRITE, cpack_len, 0) != 0)
        return -1;
    memset(send_buf, 0, cpack_len + 1);
    memset(recv_buf, 0, RESP_DATA_LEN);

    cpack = (CANPacket_t *)send_buf;
    cpack->addr = addr;
    cpack->data_len_code = 8;
    cpack->data[0] = 0x30;
    // 29-bit identifier
    if (addr >= 0x800)
        cpack->extended = 1;
    send_buf[cpack_len] = calculate_checksum(send_buf, cpack_len);

    LOG_HEXDUMP_DBG(send_buf, cpack_len + 1, "CAN_WRITE - TX: ");
    if (_send(cpack_len + 1) != 0)
        return -1;
    if (_receive(RESP_DATA_LEN) != 0)
        return -1;
    LOG_HEXDUMP_DBG(recv_buf, RESP_DATA_LEN, "CAN_WRITE - RX: ");
    if (recv_buf[0] != SPI_DACK) {
        LOG_ERR("Flow control couldn't be acked");
        return -1;
    }

    return 0;
}

static int can_read_vin(uint32_t request, uint32_t response)
{
    int count = 0;
    uint8_t vin[17];
    int vin_received = 0;
    CANPacket_t *can_packet = (CANPacket_t *)&recv_buf[3];
    const int cpack_len = sizeof(CANPacket_t) - CANPACKET_DATA_SIZE_MAX + 8;

    /* Call the transceive function */
    LOG_HEXDUMP_DBG(send_buf, 1, "CAN WRITE SEND: ");
    // while (count++ < 1000) {
    LOG_INF("Attempting to sniff vin");
    while (1) {
        if (send_header(SPI_ENDPOINT_CAN_READ, 0, cpack_len) != 0)
            return -1;

        /* Set the transmit and receive buffers */
        memset(send_buf, 0, 1);
        memset(recv_buf, 0, cpack_len + 4);
        send_buf[0] = calculate_checksum(send_buf, 0);
        LOG_HEXDUMP_DBG(send_buf, 1, "CAN_READ - TX: ");
        if (_send(1) != 0)
            return -1;
        int ack_count= 0;
        while (ack_count++ < 50) {        
            if (_receive(cpack_len + 4) != 0)
                return -1;
            k_msleep(1);
            if (recv_buf[0] == SPI_DACK)
                break;
        }
        LOG_HEXDUMP_DBG(recv_buf, cpack_len + 4, "CAN_READ - RX: ");
        if (recv_buf[0] != SPI_DACK) {
            LOG_ERR("CAN read data couldn't be acked!");
            return -1;
        }

        // Parse the VIN
        if (can_packet->addr == response) {
            // First frame, VIN is last 3 bytes
            if (can_packet->data[0] >> 4 == 0x1 && can_packet->data[2] == 0x62) {
                LOG_HEXDUMP_INF(recv_buf, cpack_len + 4, "CAN_READ - RX: ");
                memcpy(vin, &can_packet->data[5], 3);
                // Send flow control packet for rest of VIN
                can_send_flow_control(request);
                vin_received += 3;
            } else if (can_packet->data[0] >> 4 == 0x2) {
                LOG_HEXDUMP_INF(recv_buf, cpack_len + 4, "CAN_READ - RX: ");
                int to_read = MIN((sizeof(vin) - vin_received), 7);
                memcpy(&vin[vin_received], &can_packet->data[1], to_read);
                vin_received += to_read;
            }
        }

        if (vin_received == sizeof(vin))
            break;
    }

    LOG_INF("VIN (%d): %.17s", vin_received, vin);

    return 0;
}

static int can_write_read(struct spi_event *ev)
{
    if (can_write(ev->address) != 0)
        return -1;
    if (can_read_vin(ev->address, ev->response) != 0)
        return -1;

    return 0;
}

static int can_read()
{
    const int cpack_len = sizeof(CANPacket_t) - CANPACKET_DATA_SIZE_MAX + 8;

    /* Call the transceive function */
    if (send_header(SPI_ENDPOINT_CAN_READ, 0, cpack_len) != 0)
        return -1;

    /* Set the transmit and receive buffers */
    memset(send_buf, 0, 1);
    memset(recv_buf, 0, cpack_len + 4);
    send_buf[0] = calculate_checksum(send_buf, 0);
    LOG_HEXDUMP_INF(send_buf, 1, "CAN_READ - TX: ");
    if (_send(1) != 0)
        return -1;
    if (_receive(cpack_len + 4) != 0)
        return -1;
    LOG_HEXDUMP_INF(recv_buf, cpack_len + 4, "CAN_READ - RX: ");

    return 0;
}

#ifdef ENABLE_TESTING
static int spi_recovery_test(void)
{
    const int recv_len = RESP_VERSION_LEN - 5; // Something to mess up comm

    /* Set the transmit and receive buffers */
    memcpy(send_buf, version_string, REQ_VERSION_LEN);
    memset(recv_buf, 0, recv_len);

    /* Call the transceive function */
    LOG_DBG("--- Incomplete version read ---");
    if (_send(REQ_VERSION_LEN) != 0)
        return -1;
    if (_receive(recv_len) != 0)
        return -1;

    // Read all data from SPI
    LOG_DBG("--- Recovering SPI ---");
    spi_recovery();

    // Try to read the version
    LOG_DBG("--- Trying to read the version again ---");
    print_version();

    return 0;
}
#else
static int spi_recovery_test(void)
{
    LOG_DBG("SPI recovery test is disabled");
    return 0;
}
#endif

static uint8_t* spi_get_uid(void)
{
    static uint8_t panda_uid[PANDA_UID_LEN] = {0};
    if (panda_uid[0] != 0)
        return panda_uid;

    // Check if we have the version already
    if (strncpy(recv_buf, version_string, REQ_VERSION_LEN) != 0) {
        LOG_INF("Getting panda UID");
        print_version();
        memcpy(panda_uid, &recv_buf[9], PANDA_UID_LEN);
    }

    return panda_uid;
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

    switch (ev->type) {
        case SPI_COMM_VERSION:
            LOG_INF("Getting panda version");
            print_version();
            break;
        case SPI_COMM_CAN:
            LOG_INF("Testing SPI data transfer");
            can_write_read(ev);
            break;
        case SPI_COMM_HELLO:
            LOG_INF("Saying hello to Panda");
            write_to_uart();
            break;
        case SPI_COMM_RECOVERY:
            LOG_INF("Performing SPI recovery test");
            spi_recovery_test();
            break;
        case SPI_COMM_VIN_READ:
            LOG_INF("Reading the VIN");
            can_read_vin(ev->address, ev->response);
        case SPI_COMM_CAN_READ:
            LOG_INF("Reading from CAN");
            set_safety_mode(SAFETY_ALLOUTPUT);
            can_read();
        default:
            break;
    }

    // Send UART event
#ifdef CONFIG_TERMINAL_UI
    struct ui_terminal_event *ui_terminal_event = new_ui_terminal_event();
    __ASSERT(ui_terminal_event, "Not enough heap left to allocate event");
    ui_terminal_event->accept_input = true;
    APP_EVENT_SUBMIT(ui_terminal_event);
#endif

    return true;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, spi_event);

SYS_INIT(spi_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);