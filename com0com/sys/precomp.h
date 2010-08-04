/*
 * $Id$
 *
 * Copyright (c) 2004-2010 Vyacheslav Frolov
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
 * Revision 1.3  2008/12/02 16:10:09  vfrolov
 * Separated tracing and debuging
 *
 * Revision 1.2  2005/07/01 11:05:41  vfrolov
 * Removed unused headers
 *
 * Revision 1.1  2005/01/26 12:18:54  vfrolov
 * Initial revision
 *
 */

#ifndef _C0C_PRECOMP_H_
#define _C0C_PRECOMP_H_

#pragma warning(push, 3)

#include <ntddk.h>
//#include <wdm.h>
#include <ntddser.h>

#pragma warning(pop)

#ifndef NTDDI_VERSION

/* Declare stuff missing in old DDKs */

#define __drv_dispatchType(type)
#define __drv_aliasesMem

typedef NTSTATUS DRIVER_INITIALIZE(
    IN PDRIVER_OBJECT pDrvObj,
    IN PUNICODE_STRING pRegistryPath);

typedef VOID DRIVER_UNLOAD(
    IN PDRIVER_OBJECT pDrvObj);

typedef NTSTATUS DRIVER_ADD_DEVICE(
    IN PDRIVER_OBJECT pDrvObj,
    IN PDEVICE_OBJECT pPhDevObj);

typedef NTSTATUS DRIVER_DISPATCH(
    IN PDEVICE_OBJECT,
    IN PIRP);

typedef VOID DRIVER_CANCEL(
    IN PDEVICE_OBJECT pDevObj,
    IN PIRP pIrp);

typedef VOID KDEFERRED_ROUTINE(
    IN PKDPC pDpc,
    IN PVOID deferredContext,
    IN PVOID systemArgument1,
    IN PVOID systemArgument2);

NTSYSAPI NTSTATUS NTAPI ZwDeleteValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName);

#endif /* NTDDI_VERSION */

#define ENABLE_TRACING 1

#include "com0com.h"
#include "trace.h"
#include "syslog.h"
#include "halt.h"

#pragma warning(disable:4514) // unreferenced inline function has been removed

#endif /* _C0C_PRECOMP_H_ */
