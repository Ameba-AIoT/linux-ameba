// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Pinctrl support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#ifndef _DT_BINDINGS_REALTEK_AMEBA_PINFUNC_H
#define _DT_BINDINGS_REALTEK_AMEBA_PINFUNC_H

/*  define PIN modes */
#define GPIO                                     0x0
#define UART_DATA                                0x1
#define LOG_UART_RTS_CTS                         0x2
#define SPI                                      0x3
#define RTC                                      0x4
#define IR                                       0x5
#define SPI_FLASH                                0x6
#define I2C                                      0x7
#define SDIO                                     0x8
#define LEDC                                     0x9
#define PWM                                      0xa
#define SWD                                      0xb
#define AUDIO                                    0xc
#define I2S0_1                                   0xd
#define I2S2                                     0xe
#define I2S3                                     0xf
#define SPK_AUXIN                                0x10
#define DMIC                                     0x11
#define CAPTOUCH_AUX_ADC                         0x12
#define SIC                                      0x13
#define MIPI                                     0x14
#define USB                                      0x15
#define RF_CTRL                                  0x16
#define EXT_ZIGBEE                               0x17
#define BT_UART                                  0x18
#define BT_GPIO                                  0x19
#define BT_RF                                    0x1a
#define DBG_BTCOEX_GNT                           0x1b
#define HS_TIMER_TRIG                            0x1c
#define DEBUG_PORT                               0x1d
#define WAKEUP                                   0x1e

/* define Pins number*/
#define PIN_NO(port, line)	((((port) - 'A') << 5) + (line))

#define REALTEK_PINMUX(port, line, mode) (((PIN_NO(port, line)) << 8) | (mode))

#endif /* _DT_BINDINGS_REALTEK_AMEEBAD2_PINFUNC_H */

