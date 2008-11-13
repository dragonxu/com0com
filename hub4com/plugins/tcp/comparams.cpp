/*
 * $Id$
 *
 * Copyright (c) 2008 Vyacheslav Frolov
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
 * Revision 1.2  2008/10/06 12:15:14  vfrolov
 * Added --reconnect option
 *
 * Revision 1.1  2008/03/27 17:17:56  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
///////////////////////////////////////////////////////////////
namespace PortTcp {
///////////////////////////////////////////////////////////////
#include "comparams.h"
///////////////////////////////////////////////////////////////
ComParams::ComParams()
  : pIF(NULL),
    reconnectTime(rtDefault)
{
}
///////////////////////////////////////////////////////////////
ComParams::~ComParams()
{
  SetIF(NULL);
}
///////////////////////////////////////////////////////////////
void ComParams::SetIF(const char *_pIF)
{
  if (pIF)
    free(pIF);

  if (_pIF)
    pIF = _strdup(_pIF);
  else
    pIF = NULL;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
