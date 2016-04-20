/* Copyright (C) 2016 Free Software Foundation, Inc.
   Contributed by Agustina Arzille <avarzille@riseup.net>, 2016.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either
   version 3 of the license, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, see
   <http://www.gnu.org/licenses/>.
*/

#ifndef __HURD_QVAL_H__
#define __HURD_QVAL_H__   1

/* 64-bit integer that allows direct access to its low
 * and high limbs. */

union hurd_qval
{
  unsigned long long qv;
  struct
    {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
      unsigned int hi;
      unsigned int lo;
#  define hurd_qval_pair(lo, hi)   hi, lo
#else
#  define hurd_qval_pair(lo, hi)   lo, hi
      unsigned int lo;
      unsigned int hi;
#endif
    };
} __attribute__ ((aligned (8)));

#endif
