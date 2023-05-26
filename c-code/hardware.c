#include <string.h> //strncmp
#include <stdlib.h> //strtoul
#include <stdbool.h>

#include "logger.h"
#include "hardware.h"
#include "cm.h"

unsigned int g_seed= 0;


/**
 * Generates random data and stores it into the provided buffer.
 *
 * @param buf           Pointer to buffer
 * @param size          Size of data to be generated
 */
void generate_random(void* buf, size_t size)
{
    if (g_seed==0)
    {
        FILE* urandom = fopen("/dev/urandom", "r");
        while (g_seed==0)
        {
            int t= fread(&g_seed, sizeof(int), 1, urandom);

            if (t<=0)
            {
                break;
            }
        }
        fclose(urandom);
        srand(g_seed);
    }

    int idx= 0;
    while (idx<size)
    {
        *((char*)buf+idx++)= (char)rand();
    }
}

/**
 * Scans cpuinfo trying to identify how many cores are available.
 *
 * @param default_value         Default value, this is returned if we fail
 *
 * @return Number of cores
 */
int get_number_of_cores(int default_value)
{
    int res = 0;
    cm_set_t* p_set= cm_load_set(NULL, "/proc/cpuinfo", ":");

    if (NULL != p_set)
    {
        cm_domain_t* p_domain = cm_enumerate_set(p_set, NULL);

        if (NULL != p_domain)
        {
            cm_kv_t* kv = NULL;

            while ((kv = cm_enumerate_domain(p_domain, kv)))
            {
                if (NULL != kv)
                {
                    if (!strcmp(kv->key, "cpu cores"))
                    {
                        res= strtol(kv->value, NULL, 10);
                    }
                    if (!strcmp(kv->key, "flags"))
                    {
                        if (strstr(kv->value, " ht ")!=NULL ||
                            strstr(kv->value, " ht")!=NULL)
                        {
                            res*=2;
                        }
                    }

                }
                else
                {
                    break;
                }
            }
        }
        cm_remove_set(NULL, p_set);
    }

    return (res>0?res:default_value);
}

/**
 * Get local MAC address, well use it for identification purposes.
 *
 * @param mac   Pointer to a buffer to hold the mac >=6 bytes
 */
void get_mac(uint8_t* mac)
{
    char buf[128]= {0};
    FILE* fp= NULL;

    for (int i= 0; i<6; i++)
    {
        snprintf(buf, 128, "/sys/class/net/eth%d/address", i);
        fp= fopen(buf, "r");

        if (fp!=NULL)
        {
            break;
        }
    }

    if (fp==NULL)
    {
        return;
    }

    if (fread(mac, 1, 6, fp)<=0)
    {
        memset(mac, 0, 6);
    }
    fclose(fp);
}
