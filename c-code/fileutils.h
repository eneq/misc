#ifndef __FILEUTILS_H__
#define __FILEUTILS_H__

/**
 * @file
 * Utilities for file/directory operations.
 */

#include <stdbool.h>

/**
 * @brief Determine if the file/directory exists.
 *
 * @param[in] path - Path to file/directory to be tested.
 *
 * @return - True if the entity exists.
 */
bool exists(const char *path);

/**
 * @brief Determine if a path is a directory.
 *
 * @param[in] path - Path to be tested.
 *
 * @return - True if the path is a directory.
 */
bool isdir(const char *path);

/**
 * @brief Determine if a path is a regular file.
 *
 * @param[in] path - Path to be tested.
 *
 * @return - True if the path is a regular file.
 */
bool isfile(const char *path);

/**
 * @brief Determine if a file is user-executable.
 *
 * @param[in] path - Path to be tested.
 *
 * @return - True if the path is an executable regular file.
 */
bool isexe(const char *path);

#endif
