#include <string.h>

#include "str.h"
#include "hash.h"
#include "mem.h"

/**
 * Converts a numeric value to a string pointed to by buf, if the buffer is
 * too small the value will be truncated.
 *
 * @param int - Value
 * @param char* - String buffer
 * @param size_t - Size of buffer
 * @returns size_t - Length of converted string
 */
size_t itoa(int val, char* buf, size_t len)
{
    int index= 0;

    if (len<=1 || buf==NULL)
    {
        return 0;    // Need 1 for '\0'
    }

    if (val<0)
    {
        buf[index++]= '-';
        val= -val;
    }

    int limit= len-1;
    while (index<limit)
    {
        buf[index++]= (val % 10 + '0');

        if ((val/= 10)==0)
        {
            break;
        }
    }

    buf[index]= '\0';
    int ret= index--;


    int l= 0;
    char c;
    while (index>=0 && l<index)
    {
        c= buf[l];
        buf[l]= buf[index];
        buf[index]= c;

        l++;
        index--;
    }

    return ret;
}

uint32_t strid(const char *str)
{
    uint32_t retval = 0;
    if(str != NULL)
    {
        retval = hash_djb2(str, strlen(str));
    }

    return retval;
}

char *strclone(const char *str)
{
    char *retval = NULL;
    if(str != NULL)
    {
        retval = ALLOC(strlen(str) + 1);
        if(retval)
        {
            strcpy(retval, str);
        }
    }

    return retval;
}
