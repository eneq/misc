#include <stddef.h> // size_t
#include <ctype.h> // isprint
#include <string.h> // memset
#include <stdio.h> // printf

#define NBR_OF_COLUMNS 8

// cppcheck-suppress unusedFunction
void hexdump(const void* memory, size_t n)
{
    size_t i;
    char ascii[NBR_OF_COLUMNS] = {0};
    char tmp = 0;

    /**
     * Make sure that the length is a multiple of the number of cols
     * for prettier printout
     */
    if (n % NBR_OF_COLUMNS)
    {
        n += NBR_OF_COLUMNS - (n % NBR_OF_COLUMNS);
        printf("Unaligned length. Increasing length to: %zu\n", n);
    }

    for (i = 0; i < n; i++)
    {
        /* Start of every newline, print the offset in hex */
        if (0 == i % NBR_OF_COLUMNS)
        {
            printf("0x%06zu: ", i);
        }

        /* Save the ascii representation of this character */
        tmp = ((char*)memory)[i];
        ascii[i % NBR_OF_COLUMNS] = isprint(tmp) ? 0xFF & tmp : '.';

        /* Print the character in hex */
        printf("%02x ",0xFF &((char*)memory)[i]);

        /* On every end of line, print the ascii representation*/
        if (i % NBR_OF_COLUMNS == (NBR_OF_COLUMNS - 1))
        {
            printf("%s\n", ascii);
            memset(ascii, 0x00, sizeof(ascii));
        }
    }
}
