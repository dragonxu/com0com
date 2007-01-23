/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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

#ifndef _COMHUB_H
#define _COMHUB_H

///////////////////////////////////////////////////////////////
class ComPort;
class ComParams;
///////////////////////////////////////////////////////////////
typedef vector<ComPort*> ComPorts;
typedef multimap<ComPort*, ComPort*> ComPortMap;
///////////////////////////////////////////////////////////////
class ComHub
{
  public:
    ComHub(int num);

    BOOL PlugIn(int n, const char *pPath, const ComParams &comParams);
    BOOL StartAll();
    void Write(ComPort *pFromPort, LPCVOID pData, DWORD len);
    void LostReport();
    void RouteData(int iFrom, int iTo, BOOL noRoute);
    void RouteDataReport();
    int NumPorts() { return (int)ports.size(); }

  private:
    ComPorts ports;
    ComPortMap routeDataMap;
};
///////////////////////////////////////////////////////////////

#endif  // _COMHUB_H
