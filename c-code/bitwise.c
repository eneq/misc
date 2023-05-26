/**
 * This file contains any general functionality related to bit management
 */

#include "bitwise.h"

#define ABS(x) (x*((x>0)-(x<0)))

/**
 * Bit masks
 */
static const uint8_t bitmasks[]=
{
    0b00000000,
    0b00000001,
    0b00000011,
    0b00000111,
    0b00001111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b11111111,
};


/**
 * This function returns the specified bits as a uint8_t data with the
 * requested bits in the lower part of the 8 bits.
 *
 * The indata is a uint8_t pointer to a buffer, the data is interpreted as one
 * bitstream with position 0 starting on the first bit. The function
 * calculates byte position of the bits and gets the specified bits using
 * masks/shift operations.
 *
 * @param key           Pointer to byte buffer to be used as bit stream
 * @param bit_index     The index of the first bit to be extracted (left)
 * @param bit_len       The number of bits to extract, maximum is 8 atm.
 *
 * @returns The bits extracted in the lower bits of the byte
 */
uint8_t get_bits(uint8_t* key, uint32_t bit_index, uint8_t bit_len)
{
    uint32_t pos= bit_index/8;
    uint8_t idx= (bit_index-8*pos);

    uint8_t data= *(key+pos);
    int shift= ((8-bit_len)-idx);

    if (shift<0)    // Spans byte interval
    {
        int part1= bit_len-ABS(shift);
        int part2= ABS(shift);

        data= (get_bits(key, bit_index, part1))<<part2;
        data+= get_bits(key, bit_index+part1, part2);
    }
    else
    {
        data>>=((8-bit_len)-idx);
    }

    data&= bitmasks[bit_len];
    return data;
}

