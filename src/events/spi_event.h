/*
 * Copyright (c) 2024 Evercars
 */

#ifndef _SPI_EVENT_H_
#define _SPI_EVENT_H_

#include <app_event_manager.h>

typedef enum {
    SPI_COMM_NONE = 0,
    SPI_COMM_VERSION,
    SPI_COMM_CAN,
    SPI_COMM_HELLO,
    SPI_COMM_RECOVERY,
    SPI_COMM_VIN_READ,
    SPI_COMM_CAN_READ
} spi_comm_type;

struct spi_event {
    struct app_event_header header;
    // SPI Comm type
    spi_comm_type type;
    // CAN comm
    uint32_t address : 29;
    uint32_t response : 29;
};

APP_EVENT_TYPE_DECLARE(spi_event);

#endif /* _SPI_EVENT_H_ */