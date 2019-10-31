/*
   File: digPot.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Target Chip: Microchip MCP41HV51 Digital Pot IC. 5K ohms, 8-Bit.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

// Digital Pot
#define POT_I2C_ADDR 0b0111100 // Address 0x3C (7-bit), not including read/write bit.
#define POT_WIPER_ADDR (0x00 << 4)
#define POT_TCON_ADDR (0x04 << 4)

#define POT_TCON_DEF 0xff
#define POT_WR_CMD 0b00000000
#define POT_RD_CMD 0b00001100
#define POT_INC_CMD 0b00000100
#define POT_DEC_CMD 0b00001000

#define POT_MIN 0x00
#define POT_MAX 0xff
#define POT_MIN_CUR 40
#define POT_MAX_CUR 200
#define POT_MIN_OHMS 0    // Digital Pot ohms at Minimum current.
#define POT_MAX_OHMS 5000 // Digital Pot ohms at Maximum current.

// EOF