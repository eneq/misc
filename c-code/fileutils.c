#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#include "fileutils.h"

bool isdir(const char *path)
{
    bool retval = false;

    struct stat s;
    if ((path != NULL) && (stat(path, &s) >= 0))
    {
        if (S_ISDIR(s.st_mode))
        {
            retval = true;
        }
    }

    return retval;
}

bool isfile(const char *path)
{
    bool retval = false;

    struct stat s;
    if ((path != NULL) && (stat(path, &s) >= 0))
    {
        if (S_ISREG(s.st_mode))
        {
            retval = true;
        }
    }

    return retval;
}

bool exists(const char *path)
{
    struct stat s;
    return ((path != NULL) && (stat(path, &s) >= 0));
}

bool isexe(const char *path)
{
    bool retval = false;
    struct stat s;
    if ((path != NULL) && (stat(path, &s) >= 0))
    {
        //
        // Make sure it is a regular file with user-executable
        // privileges.
        //
        retval = ((s.st_mode & S_IFREG) && (s.st_mode & S_IXUSR));
    }

    return retval;
}

