#ifndef __HWCAP_H__
#define __HWCAP_H__

#include <stdint.h>

#define MAC_LENGTH 6

/**
 * Fills the buffer provided with size amount of random bytes
 * @param buf           pointer to buffer
 * @param size          size of buffer
 */
void  generate_random(void* buf, size_t size);

/**
 * Returns the nbr of CPU cores.
 * @param defaultValue - Defaultvalue if no cores are found
 *
 * @return              Number of cores on this computer
 */
int get_number_of_cores(int default_value);

/**
 * Get local MAC address, well use it for identification purposes.
 * @param mac           Atleast 6 bytes long
 */
void get_mac(uint8_t* mac);

#endif //HWCAP_H
