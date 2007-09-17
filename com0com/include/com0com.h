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
 * Revision 1.4  2007/07/03 14:25:12  vfrolov
 * Implemented pinout customization
 *
 * Revision 1.3  2006/11/03 13:06:33  vfrolov
 * Moved C0C_PORT_NAME_LEN from sys/com0com.h to include/com0com.h
 *
 * Revision 1.2  2006/11/02 15:56:38  vfrolov
 * Defined C0C_REGSTR_VAL_PORT_NUM
 *
 * Revision 1.1  2006/10/13 10:06:13  vfrolov
 * Initial revision
 *
 */

#ifndef _INCLUDE_C0C_COM0COM_H_
#define _INCLUDE_C0C_COM0COM_H_

#define C0C_BUS_DEVICE_ID           TEXT_PREF"root\\com0com"
#define C0C_PORT_DEVICE_ID          TEXT_PREF"com0com\\port"
#define C0C_SERVICE                 TEXT_PREF"com0com"
#define C0C_PREF_NT_DEVICE_NAME     TEXT_PREF"\\Device\\"

#define C0C_PORT_NAME_LEN           12
#define C0C_PREF_BUS_NAME           TEXT_PREF"CNCBUS"
#define C0C_PREF_PORT_NAME_A        TEXT_PREF"CNCA"
#define C0C_PREF_PORT_NAME_B        TEXT_PREF"CNCB"
#define C0C_PREF_DEVICE_NAME_A      C0C_PREF_NT_DEVICE_NAME C0C_SERVICE TEXT_PREF"1"
#define C0C_PREF_DEVICE_NAME_B      C0C_PREF_NT_DEVICE_NAME C0C_SERVICE TEXT_PREF"2"

#define C0C_REGSTR_VAL_PORT_NUM     TEXT_PREF"PortNum"

#define C0C_PIN_RRTS                0x00000001
#define C0C_PIN_RDTR                0x00000002
#define C0C_PIN_ROUT1               0x00000004
#define C0C_PIN_ROPEN               0x00000080
#define C0C_PIN_LRTS                0x00000100
#define C0C_PIN_LDTR                0x00000200
#define C0C_PIN_LOUT1               0x00000400
#define C0C_PIN_LOPEN               0x00008000
#define C0C_PIN_ON                  0x10000000
#define C0C_PIN_NEGATIVE            0x80000000

#endif /* _INCLUDE_C0C_COM0COM_H_ */
