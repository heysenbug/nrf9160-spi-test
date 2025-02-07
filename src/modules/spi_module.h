#ifndef _SPI_H_
#define _SPI_H_

/* SPI module buffer sizes */
#define SPI_SEND_BUF_SIZE   256
#define SPI_RECV_BUF_SIZE   256

/* Panda Defines*/
#define PANDA_UID_LEN 12

/* Panda SPI protocol defines */
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

/* SPI CAN Packet structure for communication with Panda */
#define CAN_PACKET_VERSION 4
#define CANPACKET_HEAD_SIZE 6U
#define CANPACKET_DATA_SIZE_MAX 64U

typedef struct {
  unsigned char fd : 1;
  unsigned char bus : 3;
  unsigned char data_len_code : 4;  // lookup length with dlc_to_len
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;
  unsigned int addr : 29;
  unsigned char checksum;
  unsigned char data[CANPACKET_DATA_SIZE_MAX];
} __attribute__((packed, aligned(4))) CANPacket_t;

/* SPI Comm to Panda Defines */
#define REQ_VERSION_LEN     7
#define REQ_HEADER_LEN      7
#define RESP_VERSION_LEN    25
#define RESP_HEADER_LEN     1
#define RESP_DATA_LEN       4
#define RESP_CAN_READ_LEN   sizeof(CANPacket_t) + 4

#endif /* _SPI_H_ */