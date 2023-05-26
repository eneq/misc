/**
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE.txt', which is part of this source code package.
 *
 * The content of this file has been developed by Room 5 Inc.
 */
#ifndef __BITWISE_H__
#define __BITWISE_H__

#include <stdint.h>

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
 * @param len           The number of bits to extract, maximum is 8 atm.
 *
 * @returns The bits extracted in the lower bits of the byte
 */
uint8_t get_bits(uint8_t* key, uint32_t bit_index, uint8_t bit_len);


#endif // _BITWISE_H_
