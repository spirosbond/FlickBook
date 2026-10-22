#pragma once
#include <cstring>

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned int *)(addr))
#endif
#ifndef pgm_read_float
#define pgm_read_float(addr) (*(const float *)(addr))
#endif
#ifndef pgm_read_ptr
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#endif
// GFXfont / glyph pointers are read with pgm_read_pointer; on the host they are
// 64-bit, so read a full pointer rather than a 32-bit dword.
#ifndef pgm_read_pointer
#define pgm_read_pointer(addr) (*(void *const *)(addr))
#endif
#define memcpy_P memcpy
#define memcmp_P memcmp
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strlen_P strlen
#define strcmp_P strcmp
#define sprintf_P sprintf
#define snprintf_P snprintf
