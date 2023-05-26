#ifndef __CM_H__
#define __CM_H__

#include <stdio.h>

/**
 * Support for reading a config file containing key=value pair.
 * If a line starts with a hash "#", it is considered a comment
 *
 * ============ IMPORTANT =============
 * When a cfg element has been loaded then the domain/key are fixed in memory,
 * if a value changes then only the kv elements value pointer will be
 * changed. Thus its safe todo a cfg element lookup and use the element as a
 * reference for the configuration information.
 */

#define CM_START(x) cm_defaults_t x[]= {
#define CM_END() {NULL, NULL, NULL, NULL}};
#define CM_OPTION(d,x,y,c) {d,x,CM_QUOTE(y),c},
#define CM_QUOTE(y) #y

/**
 * CM default structure
 */
typedef struct cm_defaults_kv_s {
    char* domain;
    char* key;
    char* value;
    char* comment;
} cm_defaults_t;


/**
 * CM instance referece
 */
typedef struct cm_s cm_t;

/**
 * Representation of a set
 */
typedef struct cm_set_s cm_set_t;

/**
 * Representation of a domain
 */
typedef struct cm_domain_s cm_domain_t;

/**
 * Representation of a key/value pair
 */
typedef struct
{
    char* key;
    char* value;
    char* comment;
} cm_kv_t;

/**
 * In the case a CM singleton is desired then this function can be used, if
 * NULL is ever passed as a cm reference then the singleton will be used.
 *
 * @returns CM Context
 */
cm_t* cm_singleton();


/**
 * Creates an instance of the configuration manager, this creates the holding
 * block for the persistent configuration information.
 * @returns cm_t*
 */
cm_t* cm_initialize();


/**
 * Releases a configuration manager this will release all file monitoring and
 * all resources.
 */
void cm_release(cm_t* ctx);

/**
 * Loads a default structure(array of cm_defaults_t with last entry containing
 * NULL elements) as a set
 *
 * @param cm            CM context
 * @param defaults      Default data
 * @param name          Name of the set, "defaults" if NULL
 *
 * @return A set with domains containing key value pairs
 */
cm_set_t* cm_default_set(cm_t* cm, cm_defaults_t* defaults, char* name);


/**
 * Creates a set given a file reference
 *
 * @param cm            CM context
 * @param name          Set name, can be a file name (load_set uses filename)
 *
 * @return An empty set
 */
cm_set_t* cm_create_set(cm_t* cm, char* name);


/**
 * Removes a set from the cm context
 *
 * @param cm            CM context
 * @param name          Set reference
 */
void cm_remove_set(const cm_t* cm, cm_set_t* set);

/**
 * Loads a file and returns a set with all key values
 *
 * @param cm_t* - CM reference, if NULL then a singleton will be used.
 * @param file - Filename to open
 * @param delimiter - The key value separator (usually "=")
 *
 * @return A set with domains containing key value pairs
 */
cm_set_t* cm_load_set(cm_t* cm, const char* file, const char* delimiter);


/**
 * Writes a set to a file
 * @param set - The set to write to file
 * @param filename - The filename to save
 */
void cm_write_set(const cm_set_t* set, const char* filename);


/**
 * Frees all the memory for the set
 *
 * @param The set to free resources from
 */
void cm_clear_set(cm_set_t* set);


/**
 * Merges the second set into the first set, any existing kv-pair in the
 * destination will be retained. i.e. any data in the source that does not
 * already exist in destination will be copied over.
  *
 * @param dst - The destination
 * @param src - The source
 */
void cm_merge_set(cm_set_t* dst, cm_set_t* src);


/**
 * Adds a key/value pair to a certain domain.
 * Domain will be created if not already exists.
 * Key will be replaced with value if already exists
 *
 * @param set - The set containing the domain
 * @param domain - The domain where you want to add key/value pair
 * @param key - The key to add
 * @param value - The value for the key
 */
void cm_add_key(cm_set_t* set, char* domain, char* p_key, char* p_value);


/**
 * Removes a key from a certain domain.
 *
 * @param set - The set containing the domain
 * @param domain - The domain where you want to remove the key
 * @param key - The key to remove
 */
void cm_remove_key(cm_set_t* set, char* domain, char* key);


/**
 * Returns a value from a certain key.
 *
 * @param set - The set containing the domain
 * @param domain - The domain where you want to search for a key.
 * @param key - The key to search for
 *
 * @return value of the corresponding key
 */
char* cm_lookup_value(cm_set_t* set, char* domain, char* key);


/**
 * Returns a specified domainvalue from a certain key.
 *
 * @param set - The set containing the domain
 * @param domain - The domain
 * @return domain struct
 */
cm_domain_t* cm_lookup_domain(cm_set_t* set, char* domain);

/**
 * Returns a specified set
 *
 * @param cm - The CM object containing the set
 * @param domain - The set name reference
 * @return set struct
 */
cm_set_t* cm_lookup_set(cm_t* cm, char* set);

/**
 * Removes a domain.
 *
 * @param set - The set containing the domain
 * @param domain - The domain to remove.
 *
 */
void cm_remove_domain(cm_set_t* set, char* domain);


/**
 * Writes a domain to file.
 *
 * @param set - The set containing the domain
 * @param domain - The domain you want to write to secondary storage
 * @param filename - The filename you want to write to
 *
 */
void cm_write_domain(cm_set_t* set, char* domain, const char* filename);


/**
 * Enumerates over a CM context
 *
 * @param cm - CM context to enumerate
 * @param set - The last result from this function (NULL for starters)
 *
 * @return The next set.
 */
cm_set_t* cm_enumerate(cm_t* cm, cm_set_t* set);

/**
 * Enumerates a set over all domains
 *
 * @param set - The set to enumerate
 * @param domain - The last result from this function (NULL for starters)
 *
 * @return The next domain.
 */
cm_domain_t* cm_enumerate_set(cm_set_t* set, cm_domain_t* domain);


/**
 * Enumerates a domain over all key/values
 *
 * @param domain - The domain to enumerate
 * @param keyvalue - The last result from this function (NULL for starters)
 *
 * @return The next key/value pair.
 */
cm_kv_t* cm_enumerate_domain(cm_domain_t* domain, cm_kv_t* keyvalue);


/**
 * @brief Get the name of the domain.
 *
 * @param domain - The domain.
 *
 * @return - The domain name or NULL if domain is not valid.
 */
char *cm_domain_name(cm_domain_t *domain);

#endif //CM_H

