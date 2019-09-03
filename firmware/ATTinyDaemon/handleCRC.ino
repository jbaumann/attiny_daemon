/*
   These method calculate an 8-bit CRC based on the polynome used for Dallas / Maxim
   sensors (X^8+X^5+X^4+X^0).
   A lot of implementations exist that are equally good. I took this one from
   https://www.mikrocontroller.net/topic/155115
*/


#define CRC8INIT 0x00                                  // The initalization value used for the CRC calculation
#define CRC8POLY 0x31                                  // The CRC8 polynome used: X^8+X^5+X^4+X^0

uint8_t reg = CRC8INIT;                                // Register used for calculating the CRC

/*
   This function adds the current byte of data to the existing CRC calculation in the
   variable reg.
*/
unsigned char crc8_bytecalc(uint8_t data)
{
  uint8_t i;                                           // we assume we have less than 255 bytes data
  uint8_t flag;                                        // flag for the MSB
  uint8_t polynome = CRC8POLY;

  // for each bit of the byte
  for (i = 0; i < 8; i++) {
    if (reg & 0x80) flag = 1;                          // Test MSB of the register
    else flag = 0;
    reg <<= 1;                                         // Schiebe Register 1 Bit nach Links und
    if (data & 0x80) reg |= 1;                         // Fill the LSB with the next data bit
    data <<= 1;                                        // next bit of data
    if (flag) reg ^= polynome;                         // if flag == 1 (MSB set) then XOR with polynome
  }
  return reg;
}

/*
   This function calculates the CRC8 of a msg using the function crc8_bytecalc()
*/
unsigned char crc8_message_calc(uint8_t *msg, uint8_t len)
{
  reg = CRC8INIT;
  uint8_t i;
  for (i = 0; i < len; i++) {
    crc8_bytecalc(msg[i]);      // calculate the CRC for the next byte of data and add it to reg
  }
  return crc8_bytecalc(0);      // The calculation has to be continued for the bit length of the polynome with 0 values
}

/*
   This function calculates the CRC8 of a msg using the function crc8_bytecalc()
   and writes the message followed by the crc to I2C.
*/

void write_data_crc(uint8_t *msg, uint8_t len) {
  reg = CRC8INIT;
  uint8_t i;
  crc8_bytecalc(register_number);
  for (i = 0; i < len; i++) {
    crc8_bytecalc(msg[i]);
  }
  uint8_t crc = crc8_bytecalc(0);

  Wire.write(msg, len);
  Wire.write(&crc, 1);
}
