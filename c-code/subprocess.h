#ifndef __SUBPROCESS_H__
#define __SUBPROCESS_H__

#include <sys/types.h>

/**
 * @brief Information about a subprocess.
 */
typedef struct sp_child_s sp_child_t; /*!< Subprocess handle. */

/**
 * @brief Get the child subprocess pid.
 *
 * @param child - The subprocess.
 *
 * @return - The child or zero if invalid.
 */
pid_t sp_subproc_pid(sp_child_t *child);

/**
 * @brief Get the child subprocess stdout pipe.
 *
 * @param child - The subprocess.
 *
 * @return The pipe descriptor.
 */
int sp_subproc_outfd(sp_child_t *child);

/**
 * @brief Get the child subprocess stderr pipe.
 *
 * @param child - The subprocess.
 *
 * @return The pipe descriptor.
 */
int sp_subproc_errfd(sp_child_t *child);

/**
 * @brief Start a child subprocess.
 *
 * Pipes will be created to the subprocess allowing output to
 * stdout and stderr to be retrieved.
 *
 * This function should always be followed, at some point, by a
 * call to sp_destroy in order to avoid leaving behind zombie
 * processes.
 *
 * @param[in] command - Command to execute as a subprocess.
 *
 * @return - The child subprocess or NULL on error.
 */
sp_child_t *sp_create(const char *command);

/**
 * @brief Wait for a subprocess to finish execution.
 *
 * This function will block the parent process until the child
 * finishes its execution.
 *
 * A call to sp_destroy() should always be performed after the
 * child completes its execution.
 *
 * @param[in] child - The child process to wait on.
 * @param[out] result - If not NULL, the child process exit code.
 *
 * @return Greater than zero if the child process completed or -1 if an error occurred.
 */
int sp_wait(sp_child_t *child, int *result);

/**
 * @brief Determine if a subprocess has finished execution.
 *
 * This function is non-blocking. The result value will only
 * be valid if the child process has finished.
 *
 * A call to sp_destroy() should always be performed after the
 * child completes its execution.
 *
 * @param[in] child - The child process to wait on.
 * @param[out] result - If not NULL, the child process exit code.
 *
 * @return - Zero if the child is still running, greater than zero
 *           if it has completed or less than zero if an error
 *           occurred.
 */
int sp_poll(sp_child_t *child, int *result);

/**
 * @brief Close execution of a child process.
 *
 * This function should always be called after the parent
 * has finished with the child subprocess.
 *
 * If the child is still executing, it will be killed first
 * by sending a SIGTERM. If that is unsuccessful a SIGKILL will
 * be sent next.
 *
 * @param[in] child - The child process to close.
 *
 * @return Zero on success or non-zero if a running process could not be killed.
 */
int sp_destroy(sp_child_t *child);

#endif
