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
#include "strutils.h"

NTSTATUS FdoBusPnp(
    IN PC0C_FDOBUS_EXTENSION pDevExt,
    IN PIRP                  pIrp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
  ULONG minorFunction = pIrpStack->MinorFunction;
  PDEVICE_OBJECT pLowDevObj = pDevExt->pLowDevObj; // IRP_MN_REMOVE_DEVICE deletes *pDevExt!

  status = STATUS_SUCCESS;

  switch (minorFunction) {
  case IRP_MN_QUERY_DEVICE_RELATIONS:
    if (pIrpStack->Parameters.QueryDeviceRelations.Type == BusRelations) {
      ULONG countPdos, countRelations;
      PDEVICE_RELATIONS pRelationsPrev, pRelations;
      int i;

      countPdos = 0;
      for (i = 0 ; i < 2 ; i++) {
        if (pDevExt->childs[i].pDevExt)
          countPdos++;
      }
      if (!countPdos)
        break;

      pRelationsPrev = (PDEVICE_RELATIONS)pIrp->IoStatus.Information;
      countRelations = pRelationsPrev ? pRelationsPrev->Count : 0;

      pRelations = (PDEVICE_RELATIONS)ExAllocatePool(PagedPool,
        sizeof(DEVICE_RELATIONS) + ((countPdos + countRelations - 1) * sizeof (PDEVICE_OBJECT)));

      if (!pRelations) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        break;
      }

      if (countRelations)
        RtlCopyMemory(pRelations->Objects, pRelationsPrev->Objects,
                                      countRelations * sizeof (PDEVICE_OBJECT));

      for (i = 0 ; i < 2 ; i++) {
        if (pDevExt->childs[i].pDevExt) {
          pRelations->Objects[countRelations++] = pDevExt->childs[i].pDevExt->pDevObj;
          ObReferenceObject(pDevExt->childs[i].pDevExt->pDevObj);
        }
      }

      pRelations->Count = countRelations;

      if (pRelationsPrev)
        ExFreePool(pRelationsPrev);

      pIrp->IoStatus.Information = (ULONG_PTR)pRelations;
      pIrp->IoStatus.Status = STATUS_SUCCESS;
    }
    break;
  case IRP_MN_REMOVE_DEVICE:
    RemoveFdoBus(pDevExt);
    break;
  }

  if (status == STATUS_SUCCESS) {
    TraceIrp("FdoBusPnp", pIrp, NULL, TRACE_FLAG_RESULTS);

    IoSkipCurrentIrpStackLocation(pIrp);
    status = IoCallDriver(pLowDevObj, pIrp);

    TraceCode((PC0C_COMMON_EXTENSION)pDevExt, "PNP ", codeNameTablePnp, minorFunction, &status);
  } else {
    TraceIrp("PNP", pIrp, &status, TRACE_FLAG_RESULTS);

    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
  }

  return status;
}

NTSTATUS PdoPortQueryId(
    IN PC0C_PDOPORT_EXTENSION pDevExt,
    IN PIRP                   pIrp,
    IN PIO_STACK_LOCATION     pIrpStack)
{
  NTSTATUS status;
  PWCHAR pIDs = NULL;

  switch (pIrpStack->Parameters.QueryId.IdType) {
  case BusQueryDeviceID:
    status = DupStrW(&pIDs, C0C_PORT_DEVICE_ID, FALSE);
    break;
  case BusQueryHardwareIDs:
    status = DupStrW(&pIDs, C0C_PORT_HARDWARE_IDS, TRUE);
    break;
  case BusQueryCompatibleIDs:
    status = DupStrW(&pIDs, C0C_PORT_COMPATIBLE_IDS, TRUE);
    break;
  case BusQueryInstanceID:
    status = DupStrW(&pIDs, pDevExt->portName, FALSE);
    break;
  default:
    status = pIrp->IoStatus.Status;
  }

  pIrp->IoStatus.Information = (ULONG)pIDs;

  return status;
}

NTSTATUS PdoPortQueryCaps(
    IN PC0C_PDOPORT_EXTENSION pDevExt,
    IN PIRP                   pIrp,
    IN PIO_STACK_LOCATION     pIrpStack)
{
  PDEVICE_CAPABILITIES pCaps = pIrpStack->Parameters.DeviceCapabilities.Capabilities;

  UNREFERENCED_PARAMETER(pDevExt);

  if (pCaps->Version != 1 || pCaps->Size < sizeof(DEVICE_CAPABILITIES))
    return STATUS_UNSUCCESSFUL; 


  return pIrp->IoStatus.Status;
}

NTSTATUS PdoPortQueryDevText(
    IN PC0C_PDOPORT_EXTENSION pDevExt,
    IN PIRP                   pIrp,
    IN PIO_STACK_LOCATION     pIrpStack)
{
  NTSTATUS status;

  status = STATUS_SUCCESS;

  switch (pIrpStack->Parameters.QueryDeviceText.DeviceTextType) {
  case DeviceTextDescription:
    if (!pIrp->IoStatus.Information) {
      UNICODE_STRING portText;

      RtlInitUnicodeString(&portText, NULL);
      StrAppendStr0(&status, &portText, L"Port ");
      StrAppendStr0(&status, &portText, pDevExt->portName);

      if (NT_SUCCESS(status))
        pIrp->IoStatus.Information = (UINT_PTR)portText.Buffer;
    }
    break;
  case DeviceTextLocationInformation:
    if (!pIrp->IoStatus.Information) {
      UNICODE_STRING portText;

      RtlInitUnicodeString(&portText, NULL);
      StrAppendStr0(&status, &portText, pDevExt->portName);

      if (NT_SUCCESS(status))
        pIrp->IoStatus.Information = (UINT_PTR)portText.Buffer;
    }
    break;
  default:
    status = pIrp->IoStatus.Status;
  }

  return status;
}

NTSTATUS PdoPortPnp(
    IN PC0C_PDOPORT_EXTENSION pDevExt,
    IN PIRP                   pIrp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

  switch (pIrpStack->MinorFunction) {
  case IRP_MN_QUERY_ID:
    status = PdoPortQueryId(pDevExt, pIrp, pIrpStack);
    break;
  case IRP_MN_QUERY_CAPABILITIES:
    status = PdoPortQueryCaps(pDevExt, pIrp, pIrpStack);
    break;
  case IRP_MN_QUERY_DEVICE_TEXT:
    status = PdoPortQueryDevText(pDevExt, pIrp, pIrpStack);
    break;
  case IRP_MN_QUERY_DEVICE_RELATIONS:
    switch (pIrpStack->Parameters.QueryDeviceRelations.Type) {
    case RemovalRelations:
      status = STATUS_SUCCESS;
      break;
    case BusRelations:
    case EjectionRelations:
    case PowerRelations:
    case TargetDeviceRelation:
    default:
      status = pIrp->IoStatus.Status;
    }
    break;
  case IRP_MN_REMOVE_DEVICE:
  case IRP_MN_START_DEVICE:
  case IRP_MN_STOP_DEVICE:
  case IRP_MN_QUERY_STOP_DEVICE:
  case IRP_MN_CANCEL_STOP_DEVICE:
  case IRP_MN_QUERY_REMOVE_DEVICE:
  case IRP_MN_CANCEL_REMOVE_DEVICE:
  case IRP_MN_SURPRISE_REMOVAL:
  case IRP_MN_EJECT:
    status = STATUS_SUCCESS;
    break;
  default:
    status = pIrp->IoStatus.Status;
  }

  TraceIrp("PNP", pIrp, &status, TRACE_FLAG_RESULTS);

  pIrp->IoStatus.Status = status;
  IoCompleteRequest(pIrp, IO_NO_INCREMENT);

  return status;
}

NTSTATUS FdoPortPnp(
    IN PC0C_FDOPORT_EXTENSION pDevExt,
    IN PIRP                   pIrp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
  ULONG minorFunction = pIrpStack->MinorFunction;
  PDEVICE_OBJECT pLowDevObj = pDevExt->pLowDevObj; // IRP_MN_REMOVE_DEVICE deletes *pDevExt!

  switch (minorFunction) {
  case IRP_MN_REMOVE_DEVICE:
    RemoveFdoPort(pDevExt);
    break;
  }

  IoSkipCurrentIrpStackLocation(pIrp);
  status = IoCallDriver(pLowDevObj, pIrp);

  TraceCode((PC0C_COMMON_EXTENSION)pDevExt, "PNP ", codeNameTablePnp, minorFunction, &status);

  return status;
}

NTSTATUS c0cPnpDispatch(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
  NTSTATUS status;
  PC0C_COMMON_EXTENSION pDevExt = pDevObj->DeviceExtension;

  ASSERT(IoGetCurrentIrpStackLocation(pIrp)->MajorFunction == IRP_MJ_PNP);

  TraceIrp("PNP", pIrp, NULL, TRACE_FLAG_PARAMS);

  switch (pDevExt->doType) {
  case C0C_DOTYPE_FB:
    status = FdoBusPnp((PC0C_FDOBUS_EXTENSION)pDevExt, pIrp);
    break;
  case C0C_DOTYPE_PP:
    status = PdoPortPnp((PC0C_PDOPORT_EXTENSION)pDevExt, pIrp);
    break;
  case C0C_DOTYPE_FP:
    status = FdoPortPnp((PC0C_FDOPORT_EXTENSION)pDevExt, pIrp);
    break;
  default:
    status = STATUS_NO_SUCH_DEVICE;
    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
  }

  return status;
}
