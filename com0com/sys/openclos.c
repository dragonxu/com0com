/*
 * $Id$
 *
 * Copyright (c) 2004-2005 Vyacheslav Frolov
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

NTSTATUS FdoPortOpen(IN PC0C_FDOPORT_EXTENSION pDevExt)
{
  LIST_ENTRY queueToComplete;
  C0C_BUFFER readBufNew;
  ULONG size;
  KIRQL oldIrql;

  if (InterlockedIncrement(&pDevExt->openCount) != 1) {
    InterlockedDecrement(&pDevExt->openCount);
    return STATUS_ACCESS_DENIED;
  }

  switch (MmQuerySystemSize()) {
  case MmLargeSystem:
    size = 4096;
    readBufNew.pBase = (PUCHAR)ExAllocatePool(NonPagedPool, size);
    if (readBufNew.pBase)
      break;
  case MmMediumSystem:
    size = 1024;
    readBufNew.pBase = (PUCHAR)ExAllocatePool(NonPagedPool, size);
    if (readBufNew.pBase)
      break;
  case MmSmallSystem:
    size = 128;
    readBufNew.pBase = (PUCHAR)ExAllocatePool(NonPagedPool, size);
    if (readBufNew.pBase)
      break;
  default:
    size = 0;
    readBufNew.pBase = NULL;
  }

  readBufNew.pFree = readBufNew.pBusy = readBufNew.pBase;
  readBufNew.pEnd = readBufNew.pBase + size;
  readBufNew.busy = 0;

  InitializeListHead(&queueToComplete);

  KeAcquireSpinLock(pDevExt->pIoLock, &oldIrql);

  pDevExt->pIoPortLocal->readBuf = readBufNew;

  pDevExt->pIoPortLocal->waitMask = 0;
  pDevExt->pIoPortLocal->eventMask = 0;

  UpdateHandFlow(pDevExt, &queueToComplete);

  KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);

  FdoPortCompleteQueue(&queueToComplete);

  return STATUS_SUCCESS;
}

NTSTATUS FdoPortClose(IN PC0C_FDOPORT_EXTENSION pDevExt)
{
  LIST_ENTRY queueToComplete;
  C0C_BUFFER readBuf;
  KIRQL oldIrql;

  InitializeListHead(&queueToComplete);

  KeAcquireSpinLock(pDevExt->pIoLock, &oldIrql);

  SetModemStatus(pDevExt->pIoPortRemote, C0C_MSB_CTS, FALSE);
  SetModemStatus(pDevExt->pIoPortRemote, C0C_MSB_DSR, FALSE);

  WaitComplete(pDevExt->pIoPortRemote, &queueToComplete);

  readBuf = pDevExt->pIoPortLocal->readBuf;
  RtlZeroMemory(&pDevExt->pIoPortLocal->readBuf, sizeof(pDevExt->pIoPortLocal->readBuf));

  KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);

  FdoPortCompleteQueue(&queueToComplete);

  if (readBuf.pBase)
    ExFreePool(readBuf.pBase);

  InterlockedDecrement(&pDevExt->openCount);

  return STATUS_SUCCESS;
}

NTSTATUS c0cOpen(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
  NTSTATUS status;
  PC0C_COMMON_EXTENSION pDevExt = pDevObj->DeviceExtension;

  TraceIrp("--- Open ---", pIrp, NULL, TRACE_FLAG_PARAMS);

  switch (pDevExt->doType) {
  case C0C_DOTYPE_FP:
    status = FdoPortOpen((PC0C_FDOPORT_EXTENSION)pDevExt);
    break;
  default:
    status = STATUS_INVALID_DEVICE_REQUEST;
  }

  pIrp->IoStatus.Information = 0;

  if (!NT_SUCCESS(status))
    TraceIrp("c0cOpen", pIrp, &status, TRACE_FLAG_RESULTS);

  pIrp->IoStatus.Status = status;
  IoCompleteRequest(pIrp, IO_NO_INCREMENT);

  return status;
}

NTSTATUS c0cClose(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
  NTSTATUS status;
  PC0C_COMMON_EXTENSION pDevExt = pDevObj->DeviceExtension;

  TraceIrp("--- Close ---", pIrp, NULL, TRACE_FLAG_PARAMS);

  switch (pDevExt->doType) {
  case C0C_DOTYPE_FP:
    status = FdoPortClose((PC0C_FDOPORT_EXTENSION)pDevExt);
    break;
  default:
    status = STATUS_INVALID_DEVICE_REQUEST;
  }

  pIrp->IoStatus.Information = 0;

  if (!NT_SUCCESS(status))
    TraceIrp("c0cClose", pIrp, &status, TRACE_FLAG_RESULTS);

  pIrp->IoStatus.Status = status;
  IoCompleteRequest(pIrp, IO_NO_INCREMENT);

  return status;
}

NTSTATUS c0cCleanup(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
  NTSTATUS status;
  PC0C_COMMON_EXTENSION pDevExt = pDevObj->DeviceExtension;

  TraceIrp("c0cCleanup", pIrp, NULL, TRACE_FLAG_PARAMS);

  switch (pDevExt->doType) {
  case C0C_DOTYPE_FP:
    FdoPortCancelQueues((PC0C_FDOPORT_EXTENSION)pDevExt);

    status = STATUS_SUCCESS;
    break;
  default:
    status = STATUS_INVALID_DEVICE_REQUEST;
  }

  pIrp->IoStatus.Information = 0;

  TraceIrp("c0cCleanup", pIrp, &status, TRACE_FLAG_RESULTS);

  pIrp->IoStatus.Status = status;
  IoCompleteRequest(pIrp, IO_NO_INCREMENT);

  return status;
}
