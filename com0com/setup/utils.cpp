/*
 * $Id$
 *
 * Copyright (c) 2006 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Log$
 * Revision 1.1  2006/07/28 12:16:43  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
int VSNPRINTF(char *pBuf, int size, const char *pFmt, va_list va)
{
  char buf[1024];

  int res1 = wvsprintf(buf, pFmt, va);
  buf[sizeof(buf)/sizeof(buf[0]) - 1] = 0;

  lstrcpyn(pBuf, buf, size);

  int res2 = lstrlen(pBuf);

  return res2 == res1 ? res1 : -1;
}
///////////////////////////////////////////////////////////////
int SNPRINTF(char *pBuf, int size, const char *pFmt, ...)
{
  va_list va;

  va_start(va, pFmt);

  int res1 = VSNPRINTF(pBuf, size, pFmt, va);

  va_end(va);

  return res1;
}
///////////////////////////////////////////////////////////////
BOOL StrToInt(const char *pStr, int *pNum)
{
  BOOL res = FALSE;
  int num;
  int sign = 1;

  switch (*pStr) {
    case '-':
      sign = -1;
    case '+':
      pStr++;
      break;
  }

  for (num = 0 ;; pStr++) {
    switch (*pStr) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        num = num*10 + (*pStr - '0');
        res = TRUE;
        continue;
      case 0:
        break;
      default:
        res = FALSE;
    }
    break;
  }

  if (pNum)
    *pNum = num*sign;

  return res;
}
///////////////////////////////////////////////////////////////
BusyMask::~BusyMask()
{
  if (pBusyMask)
    LocalFree(pBusyMask);
}

void BusyMask::AddNum(int num)
{
  ULONG maskNum = num/(sizeof(*pBusyMask)*8);

  if (maskNum >= busyMaskLen) {
    SIZE_T newBusyMaskLen = maskNum + 1;
    PBYTE pNewBusyMask;

    if (!pBusyMask)
      pNewBusyMask = (PBYTE)LocalAlloc(LPTR, newBusyMaskLen);
    else
      pNewBusyMask = (PBYTE)LocalReAlloc(pBusyMask, newBusyMaskLen, LMEM_ZEROINIT);

    if (pNewBusyMask) {
      pBusyMask = pNewBusyMask;
      busyMaskLen = newBusyMaskLen;
    } else {
      return;
    }
  }

  ULONG mask = 1 << (num%(sizeof(*pBusyMask)*8));

  pBusyMask[maskNum] |= mask;
}

BOOL BusyMask::IsFreeNum(int num) const
{
  ULONG maskNum = num/(sizeof(*pBusyMask)*8);

  if (maskNum >= busyMaskLen)
    return TRUE;

  ULONG mask = 1 << (num%(sizeof(*pBusyMask)*8));

  return (pBusyMask[maskNum] & mask) == 0;
}

int BusyMask::GetFirstFreeNum() const
{
  int num;

  for (num = 0 ; !IsFreeNum(num) ; num++)
    ;

  return num;
}
///////////////////////////////////////////////////////////////
