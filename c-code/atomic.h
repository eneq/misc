/**
 * This header file acts as a focal point for definitions for ATOMIC
 * operations as they are supported by the specific compilers.
 *
 * Defines the following MACROS/Inline functions
 *
 * ATOMIC_fetch(x,z)      Returns current value of x in z
 * ATOMIC_store(x,y,z)    Stores Y in X. returns old value in z
 * ATOMIC_add(x,y,z)      Returns new value after addition in z
 * ATOMIC_sub(x,y,x)      Returns new value after subtraction in z
 */

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

// Check for GCC compiler
#if defined (__GNUC__)
#if (__GNUC__ < 3) || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
#error unsupported gcc version (requires atomic support)
#endif

#if(__GNUC__ == 4 && __GNUC_MINOR__ < 7)
#define ATOMIC_fetch(x,z)   {*z= __sync_add_and_fetch(x,0);}
#define ATOMIC_store(x,y,z) {*z= __sync_val_compare_and_swap(x,*x,*y);}
#define ATOMIC_add(x,y,z)   {*z= __sync_add_and_fetch(x,y);}
#define ATOMIC_sub(x,y,z)   {*z= __sync_sub_and_fetch(x,y);}
#define ATOMIC_spin(x)      {while(__sync_fetch_and_or(x, 1));}
#define ATOMIC_release(x)   {while(__sync_xor_and_fetch(x, 1));}

#endif
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 6)

//#define ATOMIC_MEMORY_MODE __ATOMIC_RELAXED
//#define ATOMIC_MEMORY_MODE __ATOMIC_CONSUME
//#define ATOMIC_MEMORY_MODE __ATOMIC_ACQUIRE
//#define ATOMIC_MEMORY_MODE __ATOMIC_RELEASE
//#define ATOMIC_MEMORY_MODE __ATOMIC_ACQ_REL
#define ATOMIC_MEMORY_MODE __ATOMIC_SEQ_CST

#define ATOMIC_fetch(x,z)   {__atomic_load(x,z,ATOMIC_MEMORY_MODE);}
#define ATOMIC_store(x,y,z) {__atomic_exchange(x,y,z,ATOMIC_MEMORY_MODE);}
#define ATOMIC_add(x,y,z)   {*z= __atomic_add_fetch(x,y,ATOMIC_MEMORY_MODE);}
#define ATOMIC_sub(x,y,z)   {*z= __atomic_sub_fetch(x,y,ATOMIC_MEMORY_MODE);}
#define ATOMIC_spin(x)      {while(__atomic_test_and_set(x,ATOMIC_MEMORY_MODE));}
#define ATOMIC_release(x)   {__atomic_clear(x,ATOMIC_MEMORY_MODE);}

#endif


#else // Catchall
#error unsupported compiler
#endif
#endif
