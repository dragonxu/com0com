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
 */

#include "precomp.h"
#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
class State {
  public:
    State() : connect(FALSE) {}

    BOOL connect;
};
///////////////////////////////////////////////////////////////
typedef map<int, State*> PortsMap;
typedef pair<int, State*> PortPair;

class Filter {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(int nPort);

    DWORD pin;
    BOOL negative;

  private:
    PortsMap portsMap;
};

static struct {
  DWORD val;
  const char *pName;
} pin_names[] = {
  {MS_CTS_ON,   "cts"},
  {MS_DSR_ON,   "dsr"},
  {MS_RLSD_ON,  "dcd"},
  {MS_RING_ON,  "ring"},
};

Filter::Filter(int argc, const char *const argv[])
  : pin(MS_DSR_ON),
    negative(FALSE)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "connect=")) != NULL) {
      if (*pParam == '!') {
        negative = TRUE;
        pParam++;
      }
      BOOL found = FALSE;

      for (int i = 0 ; i < sizeof(pin_names)/sizeof(pin_names[0]) ; i++) {
        if (_stricmp(pParam, pin_names[i].pName) == 0) {
          pin = pin_names[i].val;
          found = TRUE;
          break;
        }
      }

      if (!found)
        cerr << "Unknown pin " << pParam << endl;
    } else {
      cerr << "Unknown option " << pArg << endl;
    }
  }
}

State *Filter::GetState(int nPort)
{
  PortsMap::iterator iPair = portsMap.find(nPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(nPort, NULL));

      iPair = portsMap.find(nPort);

      if (iPair == portsMap.end())
        return NULL;
  }

  if (!iPair->second)
    iPair->second = new State();

  return iPair->second;
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "pin2con",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Connect on pin state changing filter",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " ... --create-filter=" << GetPluginAbout()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Options:" << endl
  << "  --connect=[!]<pin>    - <pin> is cts, dsr, dcd or ring (dsr by default)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  CONNECT(TRUE)         - it will be discarded from stream." << endl
  << "  CONNECT(FALSE)        - it will be discarded from stream." << endl
  << "  MODEM_STATUS(<value>) - current state of pins" << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  CONNECT(TRUE)         - will be added on appropriate pin state changing." << endl
  << "  CONNECT(FALSE)        - will be added on appropriate pin state changing." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0:" << GetPluginAbout()->pName << " --rt-events=dsr COM1 --use-port-module=tcp 111.11.11.11:1111" << endl
  << "    - wait DSR ON from COM1 and then establish connection to 111.11.11.11:1111" << endl
  << "      and disconnect on DSR OFF." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  return (HFILTER)new Filter(argc, argv);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    int nFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  if (pInMsg->type == HUB_MSG_TYPE_CONNECT) {
    // discard any CONNECT messages from the input stream
    pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY);
  }
  else
  if (pInMsg->type == HUB_MSG_TYPE_MODEM_STATUS) {
    BOOL connect = ((pInMsg->u.val & ((Filter *)hFilter)->pin) != 0);

    if (((Filter *)hFilter)->negative)
      connect = !connect;

    State *pState = ((Filter *)hFilter)->GetState(nFromPort);

    if (!pState)
      return FALSE;

    if (pState->connect != connect) {
      pState->connect = connect;

      if (connect) {
        pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_CONNECT, TRUE);
      } else {
        pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_CONNECT, FALSE);
      }
    }
  }

  return pInMsg != NULL;
}
///////////////////////////////////////////////////////////////
static const FILTER_ROUTINES_A routines = {
  sizeof(FILTER_ROUTINES_A),
  GetPluginType,
  GetPluginAbout,
  Help,
  NULL,           // ConfigStart
  NULL,           // Config
  NULL,           // ConfigStop
  Create,
  NULL,           // Init
  InMethod,
  NULL,           // OutMethod
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;

  return plugins;
}
///////////////////////////////////////////////////////////////
