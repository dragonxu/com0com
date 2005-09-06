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
 * Revision 1.3  2005/08/24 12:50:40  vfrolov
 * Fixed IRP processing order
 *
 * Revision 1.2  2005/07/14 13:51:09  vfrolov
 * Replaced ASSERT by HALT_UNLESS
 *
 * Revision 1.1  2005/01/26 12:18:54  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"

/*
 * FILE_ID used by HALT_UNLESS to put it on BSOD
 */
#define FILE_ID 2

PC0C_IRP_STATE GetIrpState(IN PIRP pIrp)
{
  PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

  switch (pIrpStack->MajorFunction) {
  case IRP_MJ_WRITE:
    return (PC0C_IRP_STATE)&pIrpStack->Parameters.Write.Key;
  case IRP_MJ_READ:
    return (PC0C_IRP_STATE)&pIrpStack->Parameters.Read.Key;
  case IRP_MJ_DEVICE_CONTROL:
    switch (pIrpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_SERIAL_WAIT_ON_MASK:
      return (PC0C_IRP_STATE)&pIrpStack->Parameters.DeviceIoControl.Type3InputBuffer;
    }
    break;
  }

  return NULL;
}

VOID ShiftQueue(PC0C_IRP_QUEUE pQueue)
{
  if (pQueue->pCurrent) {
    PC0C_IRP_STATE pState;

    pState = GetIrpState(pQueue->pCurrent);

    HALT_UNLESS(pState);

    pQueue->pCurrent = NULL;
    pState->flags &= ~C0C_IRP_FLAG_IS_CURRENT;
  }

  if (!IsListEmpty(&pQueue->queue)) {
    PC0C_IRP_STATE pState;
    PIRP pIrp;
    PLIST_ENTRY pListEntry;

    pListEntry = RemoveHeadList(&pQueue->queue);
    pIrp = CONTAINING_RECORD(pListEntry, IRP, Tail.Overlay.ListEntry);

    pState = GetIrpState(pIrp);

    HALT_UNLESS(pState);

    pQueue->pCurrent = pIrp;
    pState->flags &= ~C0C_IRP_FLAG_IN_QUEUE;
    pState->flags |= C0C_IRP_FLAG_IS_CURRENT;
  }
}

VOID CancelRoutine(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
  PC0C_FDOPORT_EXTENSION pDevExt;
  PC0C_IRP_STATE pState;
  KIRQL oldIrql;
  PC0C_IRP_QUEUE pQueue;

  IoReleaseCancelSpinLock(pIrp->CancelIrql);

  pDevExt = pDevObj->DeviceExtension;
  pState = GetIrpState(pIrp);
  HALT_UNLESS(pState);

  pQueue = &pDevExt->pIoPortLocal->irpQueues[pState->iQueue];

  KeAcquireSpinLock(pDevExt->pIoLock, &oldIrql);

  if (pState->flags & C0C_IRP_FLAG_IN_QUEUE) {
    RemoveEntryList(&pIrp->Tail.Overlay.ListEntry);
    pState->flags &= ~C0C_IRP_FLAG_IN_QUEUE;
  }

  if (pState->flags & C0C_IRP_FLAG_IS_CURRENT)
    ShiftQueue(pQueue);

  KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);

  TraceIrp("cancel", pIrp, NULL, TRACE_FLAG_RESULTS);

  pIrp->IoStatus.Status = STATUS_CANCELLED;
  pIrp->IoStatus.Information = 0;
  IoCompleteRequest(pIrp, IO_SERIAL_INCREMENT);
}

VOID CancelQueue(PC0C_IRP_QUEUE pQueue, PLIST_ENTRY pQueueToComplete)
{
  while (pQueue->pCurrent) {
    PDRIVER_CANCEL pCancelRoutine;
    PIRP pIrp;

    pIrp = pQueue->pCurrent;

    #pragma warning(push, 3)
    pCancelRoutine = IoSetCancelRoutine(pIrp, NULL);
    #pragma warning(pop)

    ShiftQueue(pQueue);

    if (pCancelRoutine) {
      pIrp->IoStatus.Status = STATUS_CANCELLED;
      pIrp->IoStatus.Information = 0;
      InsertTailList(pQueueToComplete, &pIrp->Tail.Overlay.ListEntry);
    }
  }
}

VOID FdoPortCancelQueue(IN PC0C_FDOPORT_EXTENSION pDevExt, IN PC0C_IRP_QUEUE pQueue)
{
  LIST_ENTRY queueToComplete;
  KIRQL oldIrql;

  InitializeListHead(&queueToComplete);

  KeAcquireSpinLock(pDevExt->pIoLock, &oldIrql);

  CancelQueue(pQueue, &queueToComplete);

  KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);

  FdoPortCompleteQueue(&queueToComplete);
}

VOID FdoPortCancelQueues(IN PC0C_FDOPORT_EXTENSION pDevExt)
{
  int i;

  for (i = 0 ; i < C0C_QUEUE_SIZE ; i++)
    FdoPortCancelQueue(pDevExt, &pDevExt->pIoPortLocal->irpQueues[i]);
}

VOID FdoPortCompleteQueue(IN PLIST_ENTRY pQueueToComplete)
{
  while (!IsListEmpty(pQueueToComplete)) {
    PIRP pIrp;
    PLIST_ENTRY pListEntry;

    pListEntry = RemoveHeadList(pQueueToComplete);
    pIrp = CONTAINING_RECORD(pListEntry, IRP, Tail.Overlay.ListEntry);

    TraceIrp("complete", pIrp, &pIrp->IoStatus.Status, TRACE_FLAG_RESULTS);

    IoCompleteRequest(pIrp, IO_SERIAL_INCREMENT);
  }
}

NTSTATUS NoPending(IN PIRP pIrp, NTSTATUS status)
{
  PDRIVER_CANCEL pCancelRoutine;

  #pragma warning(push, 3)
  pCancelRoutine = IoSetCancelRoutine(pIrp, NULL);
  #pragma warning(pop)

  if (!pCancelRoutine)
    return STATUS_PENDING;

  return status;
}

NTSTATUS FdoPortStartIrp(
    IN PC0C_FDOPORT_EXTENSION pDevExt,
    IN PIRP pIrp,
    IN UCHAR iQueue,
    IN PC0C_FDOPORT_START_ROUTINE pStartRoutine)
{
  NTSTATUS status;
  KIRQL oldIrql;
  PC0C_IRP_QUEUE pQueue;
  PC0C_IRP_STATE pState;

  pState = GetIrpState(pIrp);

  HALT_UNLESS(pState);

  pState->flags = 0;
  pState->iQueue = iQueue;

  pQueue = &pDevExt->pIoPortLocal->irpQueues[iQueue];

  KeAcquireSpinLock(pDevExt->pIoLock, &oldIrql);

  #pragma warning(push, 3)
  IoSetCancelRoutine(pIrp, CancelRoutine);
  #pragma warning(pop)

  if (pIrp->Cancel) {
    status = NoPending(pIrp, STATUS_CANCELLED);
    KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);
  } else {
    if (!pQueue->pCurrent) {
      LIST_ENTRY queueToComplete;

      pQueue->pCurrent = pIrp;
      pState->flags |= C0C_IRP_FLAG_IS_CURRENT;

      InitializeListHead(&queueToComplete);
      status = pStartRoutine(pDevExt, &queueToComplete);

      if (status == STATUS_PENDING) {
        pIrp->IoStatus.Status = STATUS_PENDING;
        IoMarkIrpPending(pIrp);
      } else {
        status = NoPending(pIrp, status);
        ShiftQueue(pQueue);
      }

      KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);

      FdoPortCompleteQueue(&queueToComplete);
    } else {
      PIO_STACK_LOCATION pIrpStack;

      pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

      if (pIrpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
          pIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_WAIT_ON_MASK) {
        status = NoPending(pIrp, STATUS_INVALID_PARAMETER);
      } else {
        pIrp->IoStatus.Status = STATUS_PENDING;
        IoMarkIrpPending(pIrp);
        InsertTailList(&pQueue->queue, &pIrp->Tail.Overlay.ListEntry);
        pState->flags |= C0C_IRP_FLAG_IN_QUEUE;
        status = STATUS_PENDING;
      }

      KeReleaseSpinLock(pDevExt->pIoLock, oldIrql);
    }
  }

  return status;
}
