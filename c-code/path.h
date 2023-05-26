#ifndef __PATH_H__
#define __PATH_H__

/**
 * @file
 *
 * @brief File path name utilities.
 *
 * The concept of a path is defined by the ideas of branches and leaves. A
 * branch is defined to be the portion of the path up to but not including the
 * last directory separator.
 *
 * Conversely, a leaf is defined to be the portion of the path after the last
 * directory seperator.
 *
 * Not all paths will have a leaf or branch.
 *
 * |   Path   |   Branch   | Leaf | abs |
 * |:----..--:|:----------:|:----:|:---:|
 * | /        |    N/A     | /    | yes |
 * | foo      |    N/A     | foo  | no  |
 * | /foo     |    /       | foo  | yes |
 * | foo/bar  |    foo     | bar  | no  |
 * | /foo/bar |    /foo    | bar  | yes |
 * | .        |    N/A     | .    | no  |
 * | ..       |    N/A     | ..   | no  |
 * | ../foo   |    ..      | foo  | no  |
 *
 */

#include <stdbool.h>

/**
 * @brief Determine if the path has a leaf.
 *
 * A leaf is the last portion of a path after the last directory
 * separator. The leaf of the root path is itself.
 *
 * @param path - The path.
 *
 * @return True if the path has a leaf.
 */
bool path_has_leaf(const char *path);

/**
 * @brief Determine if a path has a branch.
 *
 * A branch is the portion of a file path preceeding the last directory
 * separator. The root directory has no branch. Likewise, any path not
 * containing at least one directory separator has no branch. 
 *
 * @param path - The path.
 *
 * @return True if the path has a branch.
 */
bool path_has_branch(const char *path);

/**
 * @brief Determine if a filename has an extension.
 *
 * The '.' is considered to be part of the extension.
 *
 * @param path - The path.
 *
 * @return True if the path has an extension.
 */
bool path_has_ext(const char *path);

/**
 * @brief Determine if a path is absolute.
 *
 * @param path - The path.
 *
 * @return True if the path is absolute.
 */
bool path_is_abs(const char *path);

/**
 * @brief Determine if a path is relative.
 *
 * @param path - The path.
 *
 * @return True if the path is relative.
 */
bool path_is_rel(const char *path);

/**
 * @brief Find the position of the path leaf.
 *
 * See path_has_leaf() for the definition of a path leaf.
 *
 * @param path - The path.
 *
 * @return - Position of the leaf or NULL if one was not found.
 */
const char *path_find_leaf(const char *path);

/**
 * @brief Find the position of the path branch.
 *
 * See path_has_branch() for the definition of a path branch.
 *
 * @param path - The path.
 *
 * @return - Position of the branch or NULL if one was not found.
 */
const char *path_find_branch(const char *path);

/**
 * @brief Find the position of the path extension.
 *
 * See path_has_extension() for the definition of a path extension.
 *
 * @param path - The path.
 *
 * @return - Position of the extension or NULL if one was not found.
 */
const char *path_find_ext(const char *path);

/**
 * @brief Get the leaf of a path.
 *
 * See path_has_leaf() for the definition of a path leaf. The caller is
 * responsible for dellocating the resulting string.
 *
 * @param path - the path.
 *
 * @return A string containing the leaf or NULL if there wasn't one.
 */
char *path_leaf(const char *path);

/**
 * @brief Get the branch of a path.
 *
 * See path_has_branch() for the definition of a path leaf. The caller is
 * responsible for dellocating the resulting string.
 *
 * @param path - the path.
 *
 * @return A string containing the branch or NULL if there wasn't one.
 */
char *path_branch(const char *path);

/**
 * @brief Get the extension of a path.
 *
 * See path_has_extension() for the definition of a path extension. The caller
 * is responsible for dellocating the resulting string.
 *
 * @param path - the path.
 *
 * @return A string containing the extension or NULL if there wasn't one.
 */
char *path_ext(const char *name);

/**
 * @brief Concatenate one path with another.
 *
 * A directory separator will automatically be added if the first path does
 * not end with one or the second one does not begin with one.
 *
 * If either path is NULL or empty the result will be the one that is valid.
 *
 * The caller is responsible for dellocating the resulting string.
 *
 * @param p1 - Base path.
 * @param p2 - Path to be concatenated to the base.
 *
 * @return - The joined path or NULL if the operation was unsuccessful.
 */
char *path_join(const char *p1, const char *p2);

#endif
