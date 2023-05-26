#include "hash.h"

uint32_t hash_djb2(const void *data, size_t size)
{
    uint32_t retval = 0;
    if((data != NULL) && (size > 0))
    {
        retval = 5381;
        const uint8_t *ptr = data;
        for(size_t i=0; i<size; ++i, ++ptr)
        {
            retval = (retval * 33) + *ptr;
        }
    }

    return retval;
}

uint32_t hash_sdbm(const void *data, size_t size)
{
    uint32_t retval = 0;
    if((data != NULL) && (size > 0))
    {
        const uint8_t *ptr = data;
        for(size_t i=0; i<size; ++i, ++ptr)
        {
            retval = *ptr + (retval << 6) + (retval << 16) - retval;
        }
    }

    return retval;
}
