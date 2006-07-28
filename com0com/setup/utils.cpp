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
