#ifndef __HASH_H__
#define __HASH_H__

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Performs djb2 hash function
 *
 * A good string hash function:
 *
 * hash(i) = hash(i-1) * 33 ^ val
 *
 * @param data - Data to be hashed.
 * @param size - Number of bytes of data.
 *
 * @return - The hash of the data bytes.
 */
uint32_t hash_djb2(const void *data, size_t size);

/**
 * @brief Performs sdbm hash function
 *
 * A general hashing function with a good distribution:
 *
 * hash(i) = hash(i-1) * 65599 + val
 *
 * @param data - Data to be hashed.
 * @param size - Number of bytes of data.
 *
 * @return - The hash of the data bytes.
 */
uint32_t hash_sdbm(const void *data, size_t size);

#endif

