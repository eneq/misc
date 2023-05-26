#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "subprocess.h"
#include "mem.h"

#define PIPE_READ_IDX 0
#define PIPE_WRITE_IDX 1

/**
 * @brief Information about a subprocess.
 */
struct sp_child_s
{
    pid_t pid; /*!< Subprocess pid. */
    int outfd; /*!< A read file descriptor for the subprocess stdout stream. */
    int errfd; /*!< A read file descriptor for the subprocess stderr stream. */
};

pid_t sp_subproc_pid(sp_child_t *child)
{
    pid_t retval = 0;
    if(child != NULL)
    {
        retval = child->pid;
    }

    return retval;
}

int sp_subproc_outfd(sp_child_t *child)
{
    int retval = -1;
    if(child != NULL)
    {
        retval = child->outfd;
    }

    return retval;
}

int sp_subproc_errfd(sp_child_t *child)
{
    int retval = -1;
    if(child != NULL)
    {
        retval = child->errfd;
    }

    return retval;
}
sp_child_t *sp_create(const char *command)
{
    sp_child_t *retval = NULL;
    if(command != NULL)
    {
        retval = ALLOC(sizeof(*retval));
        if(retval != NULL)
        {
            //
            // Initialized the child info.
            //
            retval->pid = -1;
            retval->outfd = -1;
            retval->errfd = -1;

            //
            // Try to create the output pipes.
            //
            int pstdout[2];
            int pstderr[2];
            if((pipe(pstderr) == 0) && (pipe(pstdout) == 0))
            {
                //
                // Fork off the child process.
                //
                retval->pid = fork();
                if(retval->pid == 0)
                {
                    //
                    // We are in the child process here...
                    //
                    // Redirect the child's stderr and stdout
                    // streams to the pipes we created.
                    //
                    dup2(pstderr[PIPE_WRITE_IDX], STDERR_FILENO);
                    dup2(pstdout[PIPE_WRITE_IDX], STDOUT_FILENO);

                    //
                    // Close the unused/copied file descriptors
                    //
                    close(pstderr[PIPE_READ_IDX]);
                    close(pstderr[PIPE_WRITE_IDX]);
                    close(pstdout[PIPE_READ_IDX]);
                    close(pstdout[PIPE_WRITE_IDX]);

                    //
                    // Execute the command from a bash shell.
                    //
                    execl("/bin/bash", "bash", "-c", command, NULL);

                    //
                    // If we get here the execl has failed. Output the error
                    // associated with the failure and exit with a non-zero
                    // value.
                    //
                    perror("execl: ");
                    exit(1);
                }
                else if(retval->pid > 0)
                {
                    //
                    // Finish up the parent side by closing the unused
                    // end of each of the in/out pipes between the
                    // parent and the child.
                    //
                    close(pstderr[PIPE_WRITE_IDX]);
                    retval->errfd = pstderr[PIPE_READ_IDX];
                    close(pstdout[PIPE_WRITE_IDX]);
                    retval->outfd = pstdout[PIPE_READ_IDX];
                }
                else
                {
                    //
                    // The fork failed so we shutdown all the pipes.
                    //
                    close(pstderr[PIPE_READ_IDX]);
                    close(pstderr[PIPE_WRITE_IDX]);
                    close(pstdout[PIPE_READ_IDX]);
                    close(pstdout[PIPE_WRITE_IDX]);
                    FREE(retval);
                    retval = NULL;
                }
            }
            else
            {
                FREE(retval);
                retval = NULL;
            }
        }
    }

    return retval;
}

int sp_wait(sp_child_t *child, int *result)
{
    int retval = -1;
    if((child != NULL) && (child->pid > 0))
    {
        //
        // Block until the child has terminated.
        //
        int internal_stat;
        retval = waitpid(child->pid, &internal_stat, 0);

        //
        // If the child exited normally, we get its exit
        // code.
        //
        if((retval > 0) && (result != NULL))
        {
            //
            // Check if the child process exited normally
            // and get its exit code.
            //
            if(WIFEXITED(internal_stat))
            {
                //
                // Get its exit code.
                //
                *result = WEXITSTATUS(internal_stat);
            }
            else
            {
                //
                // The child exited abnormally.
                //
                *result = -1;
            }
        }
    }

    return retval;
}

int sp_poll(sp_child_t *child, int *result)
{
    int retval = -1;
    if((child != NULL) && (child->pid > 0))
    {
        //
        // Check on the child process to see if it has
        // finished. We do so in a non-blocking manner.
        //
        int internal_stat;
        retval = waitpid(child->pid, &internal_stat, WNOHANG);

        //
        // If the child's state has changed, we can attempt
        // to get its return value.
        //
        if((retval > 0) && (result != NULL))
        {
            //
            // Check if the child process exited normally
            // and get its exit code.
            //
            if(WIFEXITED(internal_stat))
            {
                //
                // Get its exit code.
                //
                *result = WEXITSTATUS(internal_stat);
            }
            else
            {
                //
                // The child exited abnormally.
                //
                *result = -1;
            }
        }
    }

    return retval;
}

int sp_destroy(sp_child_t *child)
{
    int retval = -1;
    if((child != NULL) && (child->pid > 0))
    {
        //
        // Perform a non-blocking wait on the child pid allowing it
        // to be cleaned up properly if it finished i.e. no zombies.
        //
        retval = 0;
        if(waitpid(child->pid, NULL, WNOHANG) == 0)
        {
            //
            // Send a terminate signal to allow the process to shutdown gracefully.
            //
            retval = kill(child->pid, SIGTERM);
            if(retval != 0)
            {
                //
                // If the child could not be terminated, take a heavier
                // handed approach and kill it.
                //
                retval = kill(child->pid, SIGKILL);
            }

            //
            // If we were able to successfully kill the child we still need
            // to wait on it otherwise we will create a zombie.
            //
            if(retval == 0)
            {
                //
                // No zombies. Note that we do a blocking waitpid() call here. Doing
                // so otherwise, will result in a zombie process. This should be
                // safe, however, as we wouldn't be unless we successfully killed
                // the process.
                //
                waitpid(child->pid, NULL, 0);
            }
        }

        //
        // Shutdown the pipes regardless of whether we had issues
        // stopping a running child.
        //
        if(child->outfd >= 0)
        {
            close(child->outfd);
        }

        if(child->errfd >= 0)
        {
            close(child->errfd);
        }
    }

    //
    // Dellocate the memory.
    //
    FREE(child);

    return retval;
}
