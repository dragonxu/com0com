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

#include "precomp.h"
#include "comio.h"
#include "comport.h"
#include "comparams.h"

///////////////////////////////////////////////////////////////
static void TraceError(DWORD err, const char *pFmt, ...)
{
  va_list va;
  va_start(va, pFmt);
  vfprintf(stderr, pFmt, va);
  va_end(va);

  fprintf(stderr, " ERROR %s (%lu)\n", strerror(err), (unsigned long)err);
}
///////////////////////////////////////////////////////////////
static BOOL myGetCommState(HANDLE handle, DCB *dcb)
{
  dcb->DCBlength = sizeof(*dcb);

  if (!GetCommState(handle, dcb)) {
    TraceError(GetLastError(), "GetCommState()");
    return FALSE;
  }
  return TRUE;
}

static BOOL mySetCommState(HANDLE handle, DCB *dcb)
{
  if (!SetCommState(handle, dcb)) {
    TraceError(GetLastError(), "SetCommState()");
    return FALSE;
  }
  return TRUE;
}
///////////////////////////////////////////////////////////////
HANDLE OpenComPort(const char *pPath, const ComParams &comParams)
{
  HANDLE handle = CreateFile(pPath,
                    GENERIC_READ|GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    TraceError(GetLastError(), "OpenComPort(): CreateFile(\"%s\")", pPath);
    return INVALID_HANDLE_VALUE;
  }

  DCB dcb;

  if (!myGetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  if (comParams.BaudRate() > 0)
    dcb.BaudRate = (DWORD)comParams.BaudRate();

  if (comParams.ByteSize() > 0)
    dcb.ByteSize = (BYTE)comParams.ByteSize();

  if (comParams.Parity() >= 0)
    dcb.Parity = (BYTE)comParams.Parity();

  if (comParams.StopBits() >= 0)
    dcb.StopBits = (BYTE)comParams.StopBits();

  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  dcb.fParity = FALSE;
  dcb.fNull = FALSE;
  dcb.fAbortOnError = FALSE;
  dcb.fErrorChar = FALSE;

  if (!mySetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  COMMTIMEOUTS timeouts;

  if (!GetCommTimeouts(handle, &timeouts)) {
    TraceError(GetLastError(), "OpenComPort(): GetCommTimeouts()");
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
  timeouts.ReadTotalTimeoutConstant = MAXDWORD - 1;
  timeouts.ReadIntervalTimeout = MAXDWORD;

  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;

  if (!SetCommTimeouts(handle, &timeouts)) {
    TraceError(GetLastError(), "OpenComPort(): SetCommTimeouts()");
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  cout
      << "Open("
      << "\"" << pPath << "\", baud=" << (unsigned)dcb.BaudRate
      << ", data=" << (unsigned)dcb.ByteSize
      << ", parity=" << ComParams::ParityStr(dcb.Parity)
      << ", stop=" << ComParams::StopBitsStr(dcb.StopBits)
      << ") - OK" << endl;
  return handle;
}
///////////////////////////////////////////////////////////////
int ReadOverlapped::iNext = 0;
int WriteOverlapped::iNext = 0;
///////////////////////////////////////////////////////////////
WriteOverlapped::WriteOverlapped(ComPort &_port, const void *_pBuf, DWORD _len)
  : port(_port),
    len(_len)
{
  pBuf = new BYTE[len];

  for (DWORD i = 0 ; i < len ; i++) {
    pBuf[i] = ((const BYTE *)_pBuf)[i];
  }
}

WriteOverlapped::~WriteOverlapped()
{
  if (pBuf)
    delete [] pBuf;
}

VOID CALLBACK WriteOverlapped::OnWrite(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  WriteOverlapped &overlapped = *(WriteOverlapped *)pOverlapped;

  if (err != ERROR_SUCCESS && err != ERROR_OPERATION_ABORTED)
    TraceError(err, "WriteOverlapped::OnWrite: %s", overlapped.port.Name().c_str());

  if (overlapped.len > done)
    overlapped.port.WriteLost() += overlapped.len - done;

  overlapped.port.WriteQueued() -= overlapped.len;

  delete &overlapped;
}

BOOL WriteOverlapped::StartWrite()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  i = iNext++;

  if (!pBuf) {
    return FALSE;
  }

  if (::WriteFileEx(port.Handle(), pBuf, len, this, OnWrite)) {
    port.WriteQueued() += len;
    return TRUE;
  }
  return FALSE;
}
///////////////////////////////////////////////////////////////
VOID CALLBACK ReadOverlapped::OnRead(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  ReadOverlapped &overlapped = *(ReadOverlapped *)pOverlapped;

  if (err == ERROR_SUCCESS)
    overlapped.port.OnRead(overlapped.buf, done);
  else
    TraceError(err, "ReadOverlapped::OnRead(): %s", overlapped.port.Name().c_str());

  overlapped.StartRead();
}

BOOL ReadOverlapped::StartRead()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  i = iNext++;

  if (::ReadFileEx(port.Handle(), buf, sizeof(buf), this, OnRead)) {
    return TRUE;
  }

  TraceError(GetLastError(), "ReadOverlapped::StartRead(): ReadFileEx() %s", port.Name().c_str());
  return FALSE;
}
///////////////////////////////////////////////////////////////
