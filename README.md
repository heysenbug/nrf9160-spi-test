# nrf9160 SPI Test

The main branch initailizes the nrf9160 devkit as a spi master

To initalize the devkit as a spi slave, checkout [spi-slave](https://github.com/heysenbug/nrf9160-spi-test/tree/spi-slave) branch

## Build

```
west build --sysbuild -p always -b nrf9160dk/nrf9160/ns .
```


## Install

To flash the binary onto the board

```
west flash
```

## SPI

SPI1 pripheral is used with a high pin drive. Make sure to set NRF to `3.3V` logic level

```
CLK  -> P0.16
MOSI -> P0.17
MISO -> P0.19
CS   -> P0.18
```
