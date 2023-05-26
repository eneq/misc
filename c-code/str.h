#ifndef __STR_H__
#define __STR_H__

#include <stddef.h>
#include <stdint.h>

/**
 * Converts a numeric value to a string pointed to by buf, if the buffer is
 * too small the value will be truncated.
 *
 * @param val           Value
 * @param buf           String buffer
 * @param len           Size of buffer
 *
 * @returns length of string returned
 */
size_t itoa(int val, char* buf, size_t len);

/**
 * @brief Create an identifier for a string.
 *
 * Rather than comparing strings directly, comparing
 * integral identifiers is both fast and efficient.
 *
 * @param str - String to be operated upon.
 *
 * @return - The integral identifier for the string.
 */
uint32_t strid(const char *str);

/**
 * @brief Clone a string
 *
 * Very much like strdup() except that it uses the
 * ALLOC macro.
 *
 * The caller is responsible for releasing the allocated
 * memory using the FREE() macro.
 *
 * @param str - The string to be cloned.
 *
 * @return - The cloned string or NULL on failure.
 */
char *strclone(const char *str);

#endif // _STR_H_
