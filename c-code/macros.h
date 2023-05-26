/**
 * Generic holder for useful macros
 */

#ifndef __MACROS_H__
#define __MACROS_H__

// Parameter check macros, test X and if false return y and possible execute z
#define m_check3(x, y, z) {if (!(x)) {{z}; return y;}}
#define m_check2(x, y) {if (!(x)) {return y;}}
#define m_check1(x) {if (!(x)) {return;}}
#define m_check(x) m_check1(x)

// increases x so its divisible by 4 (32 alignment)
#define ALIGN4(x) ((x+4)&0xfffffffc)

#endif
