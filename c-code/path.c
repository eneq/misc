#include <stdio.h>
#include <string.h>

#include "path.h"
#include "mem.h"


bool path_has_leaf(const char *path)
{
    return (path_find_leaf(path) != NULL);
}

bool path_has_branch(const char *path)
{
    return (path_find_branch(path) != NULL);
}

bool path_has_ext(const char *path)
{
    return (path_find_ext(path) != NULL);
}

bool path_is_abs(const char *path)
{
    bool retval = false;
    if(path != NULL)
    {
        retval = (*path == '/');
    }

    return retval;
}

bool path_is_rel(const char *path)
{
    bool retval = false;
    if(path != NULL)
    {
        retval = (*path != '/');
    }

    return retval;
}


char *path_leaf(const char *path)
{
    char *retval = NULL;
    const char *pos = path_find_leaf(path);
    if(pos != NULL)
    {
        size_t len = strlen(path) - (pos - path);
        retval = ALLOC(len+1);
        if(retval)
        {
            strcpy(retval, pos);
        }
    }

    return retval;
}

char *path_branch(const char *path)
{
    char *retval = NULL;
    const char *pos = path_find_branch(path);
    if(pos != NULL)
    {
        size_t len = pos - path + 1;
        retval = ALLOC(len+1);
        if(retval)
        {
            strncat(retval, path, len);
        }
    }

    return retval;
}

char *path_ext(const char *name)
{
    char *retval = NULL;
    const char *pos = path_find_ext(name);
    if(pos != NULL)
    {
        size_t len = strlen(name) - (pos - name);
        retval = ALLOC(len+1);
        if(retval)
        {
            strcpy(retval, pos);
        }
    }

    return retval;
}

char *path_join(const char *p1, const char *p2)
{
    char *retval = NULL;
    if((p1 != NULL) && (p2 != NULL))
    {
        size_t len = strlen(p1);
        bool sep = false;
        if((*(p1+len-1) != '/') && (*p2 != '/'))
        {
            ++len;
            sep = true;
        }

        len += strlen(p2);

        if(len > 0)
        {
            retval = ALLOC(len+1);
            if(retval)
            {
                strcat(retval, p1);
                if(sep)
                {
                    strcat(retval, "/");
                }
                strcat(retval, p2);
            }
        }
    }
    else if(p1 != NULL)
    {
        retval = ALLOC(strlen(p1)+1);
        if(retval)
        {
            strcpy(retval, p1);
        }
    }
    else if(p2 != NULL)
    {
        retval = ALLOC(strlen(p2)+1);
        if(retval)
        {
            strcpy(retval, p2);
        }
    }

    return retval;
}

const char *path_find_leaf(const char *path)
{
    const char *retval = NULL;
    if(path != NULL)
    {
        //
        // Find the position of the last separator.
        //
        char *pos = strrchr(path, '/');
        if(pos != NULL)
        {
            //
            // Special handling for the "/" path
            //
            if((pos == path) && (*(pos+1) == 0))
            {
                retval = pos;
            }
            else
            {
                //
                // Move one position past the separator and
                // start walking down the string.
                //
                ++pos;
                char *tmp = pos;
                while(*tmp != 0)
                {
                    //
                    // If we encounter a non-seperator character
                    // then we know there is a leaf.
                    //
                    if(*tmp != '/')
                    {
                        retval = pos;
                        break;
                    }
                    ++tmp;
                }
            }
        }
        else
        {
            retval = path;
        }
    }

    return retval;
}

const char *path_find_branch(const char *path)
{
    const char *retval = NULL;
    if(path != NULL)
    {
        char *pos = strrchr(path, '/');
        if(pos != NULL)
        {
            if((pos == path) && (*(pos+1) != 0))
            {
                retval = path;
            }
            else if(pos != path)
            {
                --pos;
                char *tmp = pos;
                while(tmp >= path)
                {
                    if(*tmp != '/')
                    {
                        retval = pos;
                        break;
                    }
                    --tmp;
                }
            }
        }
    }

    return retval;
}

const char *path_find_ext(const char *path)
{
    const char *retval = NULL;
    if(path != NULL)
    {
        //
        // Find the position of the last '.' character.
        // If there isn't one or if its the first character
        // we can immediately bail out.
        //
        char *pos = strrchr(path, '.');
        if((pos != NULL) && (pos != path))
        {
            //
            // The following criteria must be hold:
            // 1) The next character must not be a '/' or the end of the string.
            // 2) The previous character is not a '.' or '/'
            //
            char *tmp1 = pos+1;
            char *tmp2 = pos-1;
            if((*tmp1 != '/') && (*tmp1 != 0) && (*tmp2 != '.') && (*tmp2 != '/'))
            {
                retval = pos;
            }
        }
    }

    return retval;
}
