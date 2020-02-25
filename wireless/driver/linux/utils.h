/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * 
 *
 * Utilities.
 *
 * Originally written by Andrey Fidrya
 *
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#define mtlk_dump(level, buf, len, str) \
    { if(RTLOG_MAX_DLEVEL >= level) \
        __mtlk_dump(buf, len, str); \
    }

void __mtlk_dump(const void *buf, uint32 len, char *str);


uint32 mtlk_shexdump (char *buffer, uint8 *data, size_t size);

char * __MTLK_IFUNC mtlk_get_token (char *str, char *buf, size_t len, char delim);

int __MTLK_IFUNC
mtlk_str_to_mac (char const *str, uint8 *addr);

/*
  Trim spaces from left side

  \param str - pointer to the string [I]

  \return
     shifted pointer to the string without spaces
*/
static __INLINE char const*
mtlk_str_ltrim(char const *str)
{
  while(str && ((*str == ' ') || (*str == '\t'))) str++;

  return str;
}

/*
  Convert character hex value to the integer number

  \param c - character with hex value [I]

  \return
     unsinged integer
*/
static __INLINE uint8
mtlk_str_x1tol(char const c)
{
  uint8 val = 0;

  if ((c >= '0') && (c <= '9'))
    val = c - '0';
  else if ((c >= 'A') && (c <= 'F'))
    val = 10 + c - 'A';
  else if ((c >= 'a') && (c <= 'f'))
    val = 10 + c - 'a';

  return val;
}

/*
  Convert two byte character hex values to the integer number

  \param str - character array with hex values [I]

  \return
     unsinged integer
*/
static __INLINE uint8
mtlk_str_x2tol(char const str[2])
{
  uint8 val;

  val   = mtlk_str_x1tol(str[0]);
  val <<= 4;
  val  |= mtlk_str_x1tol(str[1]);

  return val;
}

#endif // _UTILS_H_

