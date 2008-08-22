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
 * Revision 1.6  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.5  2008/08/11 07:15:33  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.4  2008/04/14 07:32:04  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.3  2008/04/11 14:48:42  vfrolov
 * Replaced SET_RT_EVENTS by INIT_LSR_MASK and INIT_MST_MASK
 * Replaced COM_ERRORS by LINE_STATUS
 *
 * Revision 1.2  2008/04/07 12:24:17  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/04/02 10:33:23  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
static ROUTINE_PORT_NAME_A *pPortName = NULL;
static ROUTINE_FILTER_NAME_A *pFilterName = NULL;
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
class Filter {
  public:
    Filter(int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; }
    State *GetState(int nPort);
    const char *PortName(int nPort) const { return pPortName(hHub, nPort); }
    const char *FilterName() const { return pFilterName(hHub, (HFILTER)this); }

    DWORD pin;
    BOOL negative;

  private:
    HHUB hHub;

    typedef map<int, State*> PortsMap;
    typedef pair<int, State*> PortPair;

    PortsMap portsMap;
};

static struct {
  const char *pName;
  DWORD val;
} pin_names[] = {
  {"cts",  GO_V2O_MODEM_STATUS(MODEM_STATUS_CTS)},
  {"dsr",  GO_V2O_MODEM_STATUS(MODEM_STATUS_DSR)},
  {"dcd",  GO_V2O_MODEM_STATUS(MODEM_STATUS_DCD)},
  {"ring", GO_V2O_MODEM_STATUS(MODEM_STATUS_RI)},
  {"break", GO_V2O_LINE_STATUS(LINE_STATUS_BI)},
};

Filter::Filter(int argc, const char *const argv[])
  : pin(GO_V2O_MODEM_STATUS(MODEM_STATUS_DSR)),
    negative(FALSE),
    hHub(NULL)
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

      pin = 0;

      for (int i = 0 ; i < sizeof(pin_names)/sizeof(pin_names[0]) ; i++) {
        if (_stricmp(pParam, pin_names[i].pName) == 0) {
          pin = pin_names[i].val;
          break;
        }
      }

      if (!pin)
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
  "Connect or disconnect on changing of line or modem state filter",
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
  << "  --connect=[!]<state>  - <state> is cts, dsr, dcd, ring or break (dsr by" << endl
  << "                          default)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  GET_IN_OPTS(<pOpts>)  - the value pointed by <pOpts> will be or'ed with" << endl
  << "                          the required mask to get line status and modem" << endl
  << "                          status." << endl
  << "  CONNECT(TRUE/FALSE)   - will be discarded from stream." << endl
  << "  LINE_STATUS(<val>)    - current state of line." << endl
  << "  MODEM_STATUS(<val>)   - current state of modem." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  CONNECT(TRUE/FALSE)   - will be added on appropriate state changing." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0:" << GetPluginAbout()->pName << " COM1 --use-driver=tcp 111.11.11.11:1111" << endl
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
static BOOL CALLBACK Init(
    HFILTER hFilter,
    HHUB hHub)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hHub != NULL);

  ((Filter *)hFilter)->SetHub(hHub);

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    int nFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **ppEchoMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (pInMsg->type) {
  case HUB_MSG_TYPE_GET_IN_OPTS:
    _ASSERTE(pInMsg->u.pv.pVal != NULL);
    // or'e with the required mask to get line status and modem status
    *pInMsg->u.pv.pVal |= (((Filter *)hFilter)->pin & pInMsg->u.pv.val);
    break;
  case HUB_MSG_TYPE_FAIL_IN_OPTS: {
    DWORD fail_options = (pInMsg->u.val & ((Filter *)hFilter)->pin);

    if (fail_options) {
      cerr << ((Filter *)hFilter)->PortName(nFromPort)
           << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
           << " option(s) 0x" << hex << fail_options << dec
           << " not accepted" << endl;
    }
    break;
  }
  case HUB_MSG_TYPE_CONNECT:
    // discard any CONNECT messages from the input stream
    pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY);
    break;
  case HUB_MSG_TYPE_LINE_STATUS:
  case HUB_MSG_TYPE_MODEM_STATUS:
    WORD pin;

    if (pInMsg->type == HUB_MSG_TYPE_LINE_STATUS) {
      pin = GO_O2V_LINE_STATUS(((Filter *)hFilter)->pin);
    } else {
      pin = GO_O2V_MODEM_STATUS(((Filter *)hFilter)->pin);
    }

    if ((pin & MASK2VAL(pInMsg->u.val)) == 0)
      break;

    BOOL connect = ((pInMsg->u.val & pin) != 0);

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
    break;
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
  Init,
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
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
