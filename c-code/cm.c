#include <string.h> //strtok
#include <stdio.h> //fopen
#include <stdlib.h> //strtol
#include <ctype.h> //isspace
#include <errno.h>
#include <stdbool.h>

#include "cm.h"
#include "logger.h"
#include "list.h"
#include "mem.h"

#define EMPTY_DOMAIN_NAME "global"

// Buffer size for cfg file line parsing
#define BUFFER_SIZE (1024)


/**
 */
struct cm_s
{
    const list_handle_t* sets;
};


/**
 */
struct cm_set_s
{
    const list_handle_t* domains;
    char* name;
    int ifd;
};


/**
 */
struct cm_domain_s
{
    const list_handle_t* kvs;
    char* name;
};

// CM singleton...
static cm_t* g_cm= NULL;

// Forward local decl.
static void add_set(const cm_t* cm, cm_set_t* set);
static void print_set(const cm_set_t* set, FILE* fd);
static void set_destructor(void* data);

static cm_kv_t* get_kv_from_line(char* line, const char* delimiter);
static void* iterate_kv(void* data, void* userdata);
static cm_kv_t* create_kv(cm_domain_t* p_domain, const char* key,
                          const char* value, const char* txt);
static void kv_destructor(void* data);
static void* merge_kv(void* data, void* usrdata);
static void add_kv(cm_set_t* set, char* domain, char* p_key, char* p_value, char* txt);

static void* iterate_domains(void* data, void* userdata);
static cm_domain_t* create_domain(char* name, cm_set_t* p_set);
static void domain_destructor(void* data);
static void* merge_domains(void* data, void* userdata);

static char* trimwhitespace(char* str);
static char* trimcomment(char* str);
static void* domain_string_compare(void* data, void* userdata);
static void* set_string_compare(void* data, void* userdata);
static void* key_string_compare(void* data, void* userdata);



/**
 * In the case a CM singleton is desired then this function can be used, if
 * NULL is ever passed as a cm reference then the singleton will be used.
 *
 * @returns CM Context
 */
cm_t* cm_singleton()
{
    if (g_cm==NULL)
    {
        g_cm= cm_initialize();
    }

    return g_cm;
}

/**
 * Creates an instance of the configuration manager, this creates the holding
 * block for the persistent configuration information.
 *
 * @returns CM Context
 */
cm_t* cm_initialize()
{
    cm_t* cm= ALLOC(sizeof(*cm));

    if (cm!=NULL)
    {
        memset(cm, 0, sizeof(*cm));
        cm->sets= create_list(set_destructor);
    }

    return cm;
}


/**
 * Releases a configuration manager this will release all file monitoring and
 * all resources.
 *
 * @param cm            Context
 */
void cm_release(cm_t* cm)
{
    if (cm==NULL)
    {
        if (g_cm != NULL)
        {
            cm = g_cm;
            g_cm = NULL;
        }
    }

    if (cm!=NULL)
    {
        destroy_list(cm->sets);
        FREE(cm);
    }
}


/**
 * Loads a default structure as a set
 *
 * @param cm            CM context
 * @param defaults      Default data
 * @param name          Name of the set, "defaults" if NULL
 *
 * @return A set with domains containing key value pairs
 */
cm_set_t* cm_default_set(cm_t* cm, cm_defaults_t* defaults, char* name)
{
    cm= cm==NULL?cm_singleton():cm;

    cm_set_t* set= cm_create_set(cm, name==NULL?"defaults":name);

    if (set==NULL)
    {
        return NULL;
    }

    while(defaults!=NULL && defaults->domain!=NULL)
    {
        add_kv(set, defaults->domain, defaults->key, defaults->value, defaults->comment);
        defaults++;
    }

    return set;
}


/**
 * Creates a set given a file reference
 *
 * @param cm            CM context
 * @param name          Set name, can be a file name (load_set uses filename)
 *
 * @return An empty set
 */
cm_set_t* cm_create_set(cm_t* cm, char* name)
{
    cm= cm==NULL?cm_singleton():cm;

    cm_set_t* p_set = ALLOC(sizeof(*p_set));
    if (p_set==NULL)
    {
        return NULL;
    }

    memset(p_set, 0, sizeof(*p_set));

    if (name!=NULL)
    {
        p_set->name= ALLOC(strlen(name)+1);
        if (p_set->name==NULL)
        {
            FREE(p_set);
            return NULL;
        }
        strcpy(p_set->name, name);
    }

    p_set->domains= create_list(domain_destructor);

    add_set(cm, p_set);
    return p_set;
}

/**
 * Removes a set from the cm context
 *
 * @param cm            CM context
 * @param name          Set reference
 */
void cm_remove_set(const cm_t* cm, cm_set_t* set)
{
    cm= cm==NULL?cm_singleton():cm;

    if (set==NULL)
    {
        return;
    }

    // Set destructor will do cleanup
    remove_data_by_key(cm->sets, set);
}


/**
 * Loads a file and returns a set with all key values
 *
 * @param cm            CM context
 * @param file          Filename to open
 * @param delimiter     The key value separator (usually "=")
 *
 * @return A set with domains containing key value pairs
 */
cm_set_t* cm_load_set(cm_t* cm, const char* filename, const char* delimiter)
{
    cm= cm==NULL?cm_singleton():cm;

    if (filename==NULL || delimiter==NULL)
    {
        return NULL;
    }

    // Check if the set already exists and if so return NULL
    if (cm_lookup_set(cm, (char*)filename)!=NULL)
    {
        return NULL;
    }

    FILE* file = fopen(filename, "r");
    if (file == NULL)
    {
        return NULL;
    }

    cm_set_t* p_set= cm_create_set(cm, (char*)filename);
    if (NULL == p_set)
    {
        return NULL;
    }

    char buf[BUFFER_SIZE];
    memset(buf, 0x00, BUFFER_SIZE);
    char* domain= NULL;

    while (NULL != fgets(buf, BUFFER_SIZE-1, file))
    {
        char* line= trimwhitespace(buf);
        line= trimcomment(line);

        // If we have a domain, get the name for reuse.
        if (line[0] == '[')
        {
            char* start= &line[1];
            char* end= strchr(start, ']');

            if (start!=NULL && end!=NULL)
            {
                FREE(domain);
                domain= ALLOC(end-start+1);
                strncpy(domain, start, end-start);
                domain[end-start]= '\0';
            }
        }

        cm_kv_t* keyval= get_kv_from_line(line, delimiter);
        if (keyval!=NULL)
        {
            cm_add_key(p_set, domain?domain:"root", keyval->key, keyval->value);
            kv_destructor(keyval);
        }
    }

    FREE(domain);

    if (NULL != file)
    {
        fclose (file);
    }
    else
    {
        LOG("Could not open file: %s %s\n", filename, strerror(errno));
        cm_clear_set(p_set);
        p_set = NULL;
    }

    return p_set;
}


/**
 * Writes a set to a file
 *
 * @param set           The set to write to file
 * @param filename      The filename to save, if NULL stdout
 */
void cm_write_set(const cm_set_t* set, const char* filename)
{
    if (set==NULL || (set->name==NULL && filename==NULL))
    {
        return;
    }

    FILE* fd= filename==NULL?stdout:fopen(filename, "w");
    if (NULL != fd)
    {
        fprintf(fd, "# NOTX Configuration\n#\n");

        print_set(set, fd);

        // Shouldnt close stdout.
        if (filename!=NULL)
        {
            fclose(fd);
        }
    }
    else
    {
        perror("Could not open file");
    }
}


/**
 * Frees all the memory for the set
 *
 * @param set           The set to free resources from
 */
void cm_clear_set(cm_set_t* set)
{
    if (set)
    {
        destroy_list(set->domains);
        set->domains = NULL;
    }
}

/**
 * Merges the second set into the first set. Note that the
 * source set is not freed.
 *
 * @param set1          The destination
 * @param set2          The source
 */
void cm_merge_set(cm_set_t* set1, cm_set_t* set2)
{
    if (NULL!=set1 && NULL!=set2)
    {
        iterate(set2->domains, set1, merge_domains);
    }
}


/**
 * Adds a key/value pair to a domain
 * Domain will be created if not already exists.
 * Key will be replaced with value if already exists
 *
 * @param set           The set containing the domain
 * @param domain        The domain where you want to add key/value pair
 * @param key           The key to add
 * @param value         The value for the key
 */
void cm_add_key(cm_set_t* set, char* domain, char* p_key, char* p_value)
{
    add_kv(set, domain, p_key, p_value, NULL);
}


/**
 * Removes a key from a certain domain.
 *
 * @param set           The set containing the domain
 * @param domain        The domain where you want to remove the key
 * @param key           The key to remove
 */
void cm_remove_key(cm_set_t* set, char* domain, char* key)
{
    if (NULL==set)
    {
        return;
    }

    cm_domain_t* p_domain= cm_lookup_domain(set, domain);
    if (NULL!=p_domain)
    {
        cm_kv_t* p_kv = iterate(p_domain->kvs, key, key_string_compare);

        if (NULL!=p_kv)
        {
            remove_data_by_key(p_domain->kvs, p_kv);
        }
    }
}


/**
 * Returns a value from a certain key.
 *
 * @param set           The set containing the domain
 * @param domain        The domain where you want to search for a key.
 * @param key           The key to search for
 *
 * @return value of the corresponding key
 */
char* cm_lookup_value(cm_set_t* set, char* domain, char* key)
{
    if (domain==NULL)
    {
        domain= EMPTY_DOMAIN_NAME;
    }

    cm_domain_t* p_domain= cm_lookup_domain(set, domain);

    if (p_domain==NULL)
    {
        return NULL;
    }

    cm_kv_t* p_kv = iterate(p_domain->kvs, key, key_string_compare);
    if (NULL == p_kv)
    {
        return NULL;
    }

    return p_kv->value;
}


/**
 * Returns a specified domain
 *
 * @param set           The set containing the domain
 * @param domain        The domain
 *
 * @return domain struct
 */
cm_domain_t* cm_lookup_domain(cm_set_t* set, char* domain)
{
    bool enumerate= set==NULL?true:false;

    if (enumerate && (set= cm_enumerate(NULL, NULL))==NULL)
    {
        return NULL;
    }

    if (domain==NULL)
    {
        domain= EMPTY_DOMAIN_NAME;
    }

    cm_domain_t* p_domain= NULL;
    while(set!=NULL)
    {
        p_domain= iterate(set->domains, domain, domain_string_compare);

        if (p_domain!=NULL)
        {
            break;
        }

        set= enumerate?cm_enumerate(NULL, set):NULL;
    }
    return p_domain;
}


/**
 * Returns a specified set
 *
 * @param cm            The CM object containing the set
 * @param name          The set name reference, if NULL the first will be returned
 *
 * @return set struct
 */
cm_set_t* cm_lookup_set(cm_t* cm, char* set)
{
    cm= cm==NULL?cm_singleton():cm;

    if (NULL==set)
    {
        return cm_enumerate(cm, NULL);
    }

    cm_set_t* p_set = iterate(cm->sets, set, set_string_compare);

    return p_set;
}


/**
 * Removes a domain.
 *
 * @param set           The set containing the domain
 * @param domain        The domain to remove.
 *
 */
void cm_remove_domain(cm_set_t* set, char* domain)
{
    bool enumerate= set==NULL?true:false;

    if (enumerate && (set= cm_enumerate(NULL, NULL))==NULL)
    {
        return;
    }

    cm_domain_t* p_domain= NULL;
    while (set!=NULL)
    {
        p_domain= iterate(set->domains, domain, domain_string_compare);

        if (p_domain!=NULL)
        {
            break;
        }

        set= enumerate?cm_enumerate(NULL, set):NULL;
    }


    if (NULL != p_domain)
    {
        remove_data_by_key(set->domains, p_domain);
    }
}


/**
 * Enumerates over a CM context
 *
 * @param cm            CM context to enumerate
 * @param set           The last result from this function (NULL for starters)
 *
 * @return The next set.
 */
cm_set_t* cm_enumerate(cm_t* cm, cm_set_t* set)
{
    cm= cm==NULL?cm_singleton():cm;
    return get_next(cm->sets, set);
}


/**
 * Enumerates a set over all domains
 *
 * @param set           The set to enumerate
 * @param domain        The last result from this function (NULL for starters)
 *
 * @return The next domain.
 */
cm_domain_t* cm_enumerate_set(cm_set_t* set, cm_domain_t* domain)
{
    if (set==NULL && (set= cm_enumerate(NULL, NULL))==NULL)
    {
        return NULL;
    }

    return get_next(set->domains, domain);
}


/**
 * Enumerates a domain over all key/values
 *
 * @param domain        The domain to enumerate
 * @param kv            The last result from this function (NULL for starters)
 *
 * @return The next key/value pair.
 */
cm_kv_t* cm_enumerate_domain(cm_domain_t* domain, cm_kv_t* kv)
{
    if (domain==NULL)
    {
        return NULL;
    }

    return get_next(domain->kvs, kv);
}


/**
 * Returns a domains name
 *
 * @param domain        The domain to enumerate
 *
 * @return Character string
 */
char* cm_domain_name(cm_domain_t *domain)
{
    if(domain == NULL)
    {
        return NULL;
    }

    return domain->name;
}


/**
 * Adds a set to the relevant cm instance.  If the cm instance is
 * unspecified, the existing cm singleton will be populated with
 * the set provided.
 *
 * @param cm            Relevant cm instance. Can be NULL.
 * @param set           The set to add
 */
static void add_set(const cm_t* cm, cm_set_t* set)
{
    if (set==NULL)
    {
        return;
    }

    cm= cm==NULL?cm_singleton():cm;
    add_to_tail(cm->sets, set);
}


/**
 * Adds a key to a data set
 *
 * @param set           The set to modify
 * @param domain        Domain to modify
 * @param p_key         Value key
 * @param p_value       Value
 * @param txt           Value information
 */
static void add_kv(cm_set_t* set, char* domain, char* p_key, char* p_value, char* txt)
{
    domain= domain==NULL?EMPTY_DOMAIN_NAME:domain;
    cm_domain_t* p_domain= cm_lookup_domain(set, domain);

    if (NULL==p_domain)
    {
        if (set==NULL)
        {
            return;
        }

        p_domain = create_domain(domain, set);
    }

    cm_kv_t* p_kv = iterate(p_domain->kvs, p_key, key_string_compare);
    if (NULL == p_kv)
    {
        p_kv = create_kv(p_domain, p_key, p_value, txt);
    }
    else
    {
        FREE(p_kv->value);
        p_kv->value= ALLOC(strlen(p_value)+1);
        strcpy(p_kv->value, p_value);
    }
}


/**
 * Iterate function to write key value data to file
 *
 * @param data          Kv pair
 * @param userdata      File descriptor
 *
 * @return Pointer, NULL continues anything else stops iteration.
 */
static void* iterate_kv(void* data, void* userdata)
{
    if (NULL == data || NULL == userdata)
    {
        return NULL;
    }

    cm_kv_t* p_kv = (cm_kv_t*)data;
    FILE* fd = (FILE*) userdata;

    if (p_kv->comment!=NULL)
    {
        fprintf(fd, "\n%s\n", p_kv->comment);
    }
    fprintf(fd, "%s = %s\n", p_kv->key, p_kv->value);
    return NULL;// Dont stop the iteration
}


/**
 */
static void* iterate_domains(void* data, void* userdata)
{
    cm_domain_t* p_domain = (cm_domain_t*)data;
    FILE* fd = (FILE*) userdata;

    fprintf(fd, "\n[%s]", p_domain->name);
    iterate(p_domain->kvs, fd, iterate_kv);
    return NULL; // Dont stop the iteration
}


/**
 * Iterate function for iterating over domains
 *
 * @param set           CM reference
 * @param fd            File descriptor
 */
static void print_set(const cm_set_t* set, FILE* fd)
{
    if (NULL != set && NULL != set->domains && NULL != fd)
    {
        iterate(set->domains, fd, iterate_domains);
    }
}


/**
 * Removes white spaces/tabs form string.
 *
 * @param str           String to be cleaned
 *
 * @return Cleaned string
 */
static char* trimwhitespace(char* str)
{
    if (NULL == str)
    {
        return NULL;
    }

    while (isspace(*str))
    {
        str++;
    }

    if (*str != 0)
    {
        char* end;
        end = str + strlen(str) - 1;
        while (end > str && isspace(*end))
        {
            end--;
        }

        *(end+1) = 0;
    }

    return str;
}


/**
 * Removes '#' comments from string
 *
 * @param str           String to be cleaned
 *
 * @return cleaned
 */
static char* trimcomment(char* str)
{
    if (str==NULL)
    {
        return NULL;
    }

    int idx= 0;
    int len= strlen(str);

    while (idx<len && *(str+idx)!='#' && *(str+idx)!='\0')
    {
        idx++;
    }
    *(str+idx)= '\0';

    return str;
}


/**
 * Destructor call, used to free individual kv pairs
 *
 * @param data          keyvalue data
 */
static void kv_destructor(void* data)
{
    cm_kv_t* p_kv= (cm_kv_t*)data;

    FREE(p_kv->key);
    FREE(p_kv->value);
    FREE(p_kv->comment);
    FREE(p_kv);
}


/**
 * Destructor call, used to clean domains
 *
 * @param data          Domain reference
 */
static void domain_destructor(void* data)
{
    cm_domain_t* p_domain = (cm_domain_t*)data;
    destroy_list(p_domain->kvs);

    FREE(p_domain->name);
    FREE(p_domain);
}


/**
 * Destructor call, used to clean whole sets
 *
 * @param data          Set reference
 */
static void set_destructor(void* data)
{
    cm_set_t* p_set = (cm_set_t*)data;
    destroy_list(p_set->domains);

    FREE(p_set->name);
    FREE(p_set);
}


/**
 * Parses input line and extrace any Key Value pair from it given the
 * delimiter.
 *
 * @param line          Input line
 * @param delimiter     Delimiter
 *
 * @return KV pair data
 */
static cm_kv_t* get_kv_from_line(char* line, const char* delimiter)
{
    if (NULL == line || NULL == delimiter)
    {
        return NULL;
    }

    cm_kv_t* kv= ALLOC(sizeof(*kv));

    if (kv != NULL)
    {
        char* key= strtok(line, delimiter);
        char* value= strtok(NULL, delimiter);
        memset(kv, 0x00, sizeof(*kv));

        if (key!=NULL && value!=NULL && value>key)
        {
            *(value-1)='\0';

            key= trimwhitespace(key);
            value= trimwhitespace(value);

            kv->key= ALLOC((value-key)); // +1 included due to delimiter
            kv->value= ALLOC(strlen(value)+1);

            strcpy(kv->key, key);
            strcpy(kv->value, value);
        }
        else
        {
            FREE(kv);
        }
    }
    return kv;
}


/**
 * Creates a kv pair element and adds it to the specified domain.
 *
 * @param p_domain      Domain reference
 * @param key           Key value
 * @param value         Data value
 * @param txt           Value comment
 *
 * @return KV pair
 */
static cm_kv_t* create_kv(cm_domain_t* p_domain, const char* key,
                          const char* value, const char* txt)
{
    cm_kv_t* kv = NULL;

    kv= ALLOC(sizeof(*kv));

    if (NULL != kv)
    {
        kv->key= ALLOC(strlen(key)+1);
        kv->value= ALLOC(strlen(value)+1);

        strcpy(kv->key, key);
        strcpy(kv->value, value);

        if (txt!=NULL)
        {
            kv->comment= ALLOC(strlen(txt)+1);
            strcpy(kv->comment, txt);
        }
        add_to_tail(p_domain->kvs, kv);
    }

    return kv;
}


/**
 * Creates a domain with a given name within a KV set
 *
 * @param name          Name of domain
 * @param p_set         CM Set reference
 *
 * @return Pointer to domain reference
 */
static cm_domain_t* create_domain(char* name, cm_set_t* p_set)
{
    cm_domain_t* p_domain= cm_lookup_domain(p_set, name);

    if (p_domain==NULL)
    {
        p_domain = ALLOC(sizeof(*p_domain));
        memset(p_domain, 0, sizeof(*p_domain));

        p_domain->name= ALLOC(strlen(name)+1);
        strcpy(p_domain->name, name);

        p_domain->kvs = create_list(kv_destructor);
        add_to_tail(p_set->domains, p_domain);
    }

    return p_domain;
}


/**
 * Merges a kv list into a domain
 *
 * @param data          KV list
 * @param userdata      Domain
 *
 * @return NULL pointer to continue
 */
static void* merge_kv(void* data, void* userdata)
{
    cm_kv_t* p_kv= (cm_kv_t*)data;
    cm_domain_t* p_domain= (cm_domain_t*)userdata;

    // See if the key exists in the dest domain and if so exit.
    cm_kv_t* p_dst= iterate(p_domain->kvs, p_kv->key, key_string_compare);
    if (NULL!=p_dst)
    {
        return NULL;
    }

    p_dst= ALLOC(sizeof(*p_dst));

    p_dst->key= ALLOC(strlen(p_kv->key)+1);
    strcpy(p_dst->key, p_kv->key);

    p_dst->value= ALLOC(strlen(p_kv->value)+1);
    strcpy(p_dst->value, p_kv->value);

    if (p_kv->comment!=NULL)
    {
        p_dst->comment= ALLOC(strlen(p_kv->comment)+1);
        strcpy(p_dst->comment, p_kv->comment);
    }

    add_to_tail(p_domain->kvs, p_dst);
    return NULL;// Dont stop the iteration
}


/**
 * Merges a domain into a provided CM set
 *
 * @param data          Domain reference
 * @param userdata      Set reference
 *
 * @return NULL pointer to continue
 */
static void* merge_domains(void* data, void* userdata)
{
    cm_domain_t* p_domain = (cm_domain_t*)data;
    cm_set_t* p_set = (cm_set_t*)userdata;

    // if domain exists its returned in the create
    cm_domain_t* p_copy = create_domain(p_domain->name,p_set);
    iterate(p_domain->kvs, p_copy, merge_kv);
    return NULL; // Dont stop the iteration
}


/**
 * Compares to domain names
 *
 * @param data          Domain reference
 * @param userdata      Domain name (char*)
 *
 * @return
 */
static void* domain_string_compare(void* data, void* userdata)
{
    void* result = NULL;

    if (NULL == data || NULL == userdata)
    {
        return NULL;
    }

    char* str1 = ((cm_domain_t*)data)->name;
    char* str2 = (char*)userdata;

    if (!strcmp(str1, str2))
    {
        result = data;
    }
    return result;
}


/**
 * Compares to set names
 *
 * @param data          Set reference
 * @param userdata      Set name (char*)
 *
 * @return
 */
static void* set_string_compare(void* data, void* userdata)
{
    void* result = NULL;

    if (NULL == data || NULL == userdata)
    {
        return NULL;
    }

    char* str1 = ((cm_set_t*)data)->name;
    char* str2 = (char*)userdata;

    if (!strcmp(str1, str2))
    {
        result = data;
    }
    return result;
}


/**
 * Compares two key names
 *
 * @param data          KV Pair structure reference
 * @param userdata      String
 *
 * @return NULL if no match otherwise the data is returned.
 */
static void* key_string_compare(void* data, void* userdata)
{
    void* result = NULL;

    if (NULL == data || NULL == userdata)
    {
        return NULL;
    }

    char* str1 = ((cm_kv_t*)data)->key;
    char* str2 = (char*)userdata;

    if (!strcasecmp(str1, str2))
    {
        result = data;
    }
    return result;
}

