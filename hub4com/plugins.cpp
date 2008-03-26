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
 *
 */

#include "precomp.h"
#include "port.h"
#include "comhub.h"
#include "plugins.h"
#include "bufutils.h"
#include "hubmsg.h"
#include "export.h"

///////////////////////////////////////////////////////////////
class PluginEnt {
  public:
    PluginEnt(const PLUGIN_ROUTINES_A *_pRoutines);
    ~PluginEnt();

    void Help(const char *pProgPath) const;
    BOOL Config(const char *pArg) const;

    #define ABOUT(item) \
    string item() const { \
      if (!ROUTINE_IS_VALID(pRoutines, pGetPluginAbout)) \
        return ""; \
      const PLUGIN_ABOUT_A *pAbout = pRoutines->pGetPluginAbout(); \
      if (!pAbout || !ROUTINE_IS_VALID(pAbout, p##item)) \
        return ""; \
      return pAbout->p##item; \
    }

    ABOUT(Name)
    ABOUT(Copyright)
    ABOUT(License)
    ABOUT(Description)

    #undef ABOUT

    const PLUGIN_ROUTINES_A *Routines(HCONFIG *phConfig) {
      inUse = TRUE;
      *phConfig = hConfig;
      return pRoutines;
    }

    BOOL InUse() const { return inUse; }

  private:
    const PLUGIN_ROUTINES_A *pRoutines;
    BOOL inUse;
    HCONFIG hConfig;
};

PluginEnt::PluginEnt(const PLUGIN_ROUTINES_A *_pRoutines)
  : pRoutines(_pRoutines),
    inUse(FALSE),
    hConfig(NULL)
{
  if (ROUTINE_IS_VALID(pRoutines, pConfigStart))
    hConfig = pRoutines->pConfigStart();
}

PluginEnt::~PluginEnt()
{
  if (hConfig && ROUTINE_IS_VALID(pRoutines, pConfigStop))
    pRoutines->pConfigStop(hConfig);
}

void PluginEnt::Help(const char *pProgPath) const
{
  if (ROUTINE_IS_VALID(pRoutines, pHelp))
    pRoutines->pHelp(pProgPath);
  else
    cerr << "No help found." << endl;
}

BOOL PluginEnt::Config(const char *pArg) const
{
  if (hConfig && ROUTINE_IS_VALID(pRoutines, pConfig))
    return pRoutines->pConfig(hConfig, pArg);

  return FALSE;
}
///////////////////////////////////////////////////////////////
static string type2str(PLUGIN_TYPE type)
{
  stringstream str;

  switch (type) {
    case PLUGIN_TYPE_FILTER: str << "filter"; break;
    case PLUGIN_TYPE_PORT:   str << "port";   break;
    default:                 str << type;
  }

  return str.str();
}
///////////////////////////////////////////////////////////////
static BOOL GetPluginsDir(string &pluginsDir)
{
  DWORD res;
  char *pPath1 = new char[MAX_PATH];

  if (!pPath1) {
    return FALSE;
  }

  res = GetModuleFileName(NULL, pPath1, MAX_PATH);

  if (!res || res >= MAX_PATH) {
    delete [] pPath1;
    return FALSE;
  }

  char *pPath2 = new char[MAX_PATH];

  if (!pPath2) {
    delete [] pPath1;
    return FALSE;
  }

  char *pName;

  res = GetFullPathName(pPath1, MAX_PATH, pPath2, &pName);

  delete [] pPath1;

  if (!res || res >= MAX_PATH || !pName) {
    delete [] pPath2;
    return FALSE;
  }

  *pName = 0;

  pluginsDir = pPath2;

  delete [] pPath2;

  pluginsDir += "plugins\\";

  return TRUE;
}
///////////////////////////////////////////////////////////////
Plugins::Plugins()
{
  string pluginsDir;

  if (!GetPluginsDir(pluginsDir))
    return;

  string pathWildcard(pluginsDir);

  pathWildcard += "*.dll";

  WIN32_FIND_DATA findFileData;

  HANDLE hFind = FindFirstFile(pathWildcard.c_str(), &findFileData);

  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do {
    string pathPlugin(pluginsDir);

    pathPlugin += findFileData.cFileName;

    LoadPlugin(pathPlugin);

  } while (FindNextFile(hFind, &findFileData));

  FindClose(hFind);
}
///////////////////////////////////////////////////////////////
Plugins::~Plugins()
{
  for (DllPluginsArray::const_iterator iDll = dllPluginsArray.begin() ; iDll != dllPluginsArray.end() ; iDll++) {
    BOOL inUse = FALSE;

    if (!*iDll)
      continue;

    if ((*iDll)->second) {
      for (PluginArray::const_iterator i = (*iDll)->second->begin() ; i != (*iDll)->second->end() ; i++) {
        if (*i && (*i)->InUse()) {
          inUse = TRUE;
          break;
        }
      }

      delete (*iDll)->second;
    }

    if (inUse)
      (*iDll)->first = NULL;
  }

  for (TypePluginsMap::const_iterator iPair = plugins.begin() ; iPair != plugins.end() ; iPair++) {
    if (iPair->second) {
      for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++) {
        if (*i)
          delete *i;
      }

      delete iPair->second;
    }
  }

  for (DllPluginsArray::const_iterator iDll = dllPluginsArray.begin() ; iDll != dllPluginsArray.end() ; iDll++) {
    if (!*iDll)
      continue;

    if ((*iDll)->first)
      FreeLibrary((*iDll)->first);
  }
}
///////////////////////////////////////////////////////////////
void Plugins::LoadPlugin(const string &pathPlugin)
{
  HMODULE hDll = ::LoadLibrary(pathPlugin.c_str());

  if (!hDll) {
    cerr << "Can't load " << pathPlugin << endl;
    return;
  }

  PLUGIN_INIT_A *pInitProc;

  pInitProc = (PLUGIN_INIT_A *)::GetProcAddress(hDll, PLUGIN_INIT_PROC_NAME_A);

  if (!pInitProc) {
    pInitProc = (PLUGIN_INIT_A *)::GetProcAddress(hDll, PLUGIN_INIT_PROC_NAME);

    if (!pInitProc) {
      cerr << "No procedure " << PLUGIN_INIT_PROC_NAME_A << " in " << pathPlugin << endl;
      FreeLibrary(hDll);
      return;
    }
  }

  DllPlugins *pDllPlugins = new DllPlugins(hDll, new PluginArray);

  if (!pDllPlugins || !pDllPlugins->second) {
    cerr << "No enough memory." << endl;

    if (pDllPlugins)
      delete pDllPlugins;

    FreeLibrary(hDll);
    return;
  }

  InitPlugin(pathPlugin, pInitProc, *pDllPlugins->second);

  if (pDllPlugins->second->size()) {
    dllPluginsArray.push_back(pDllPlugins);
  } else {
    delete pDllPlugins->second;
    delete pDllPlugins;
    FreeLibrary(hDll);
  }
}
///////////////////////////////////////////////////////////////
void Plugins::InitPlugin(
    const string &moduleName,
    PLUGIN_INIT_A *pInitProc,
    PluginArray &pluginArray)
{
  for (const PLUGIN_ROUTINES_A *const *ppPlgRoutines = pInitProc(&hubRoutines) ;
       *ppPlgRoutines ;
       *ppPlgRoutines++)
  {
    PLUGIN_TYPE type = ROUTINE_IS_VALID(*ppPlgRoutines, pGetPluginType) ?
                       (*ppPlgRoutines)->pGetPluginType() :
                       PLUGIN_TYPE_INVALID;

    if (type == PLUGIN_TYPE_INVALID) {
      cerr << "Found plugin with invalid type in " << moduleName << endl;
      continue;
    }

    PluginEnt *pPlugin = new PluginEnt(*ppPlgRoutines);

    if (!pPlugin) {
      cerr << "No enough memory." << endl;
      continue;
    }

    TypePluginsMap::iterator iPair = plugins.find(type);

    if (iPair == plugins.end()) {
      plugins.insert(pair<PLUGIN_TYPE, PluginArray*>(type, NULL));

      iPair = plugins.find(type);

      if (iPair == plugins.end()) {
        cerr << "Can't add plugin type " << type << endl;
        delete pPlugin;
        continue;
      }
    }

    if (!iPair->second) {
      iPair->second = new PluginArray;

      if (!iPair->second) {
        cerr << "No enough memory." << endl;
        delete pPlugin;
        continue;
      }
    }

    for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++) {
      if (*i && (*i)->Name() == pPlugin->Name()) {
        cerr
          << "Plugin " << pPlugin->Name() << " with type " << type
          << " already exists. Ignored plugin in " << moduleName << endl;
        delete pPlugin;
        pPlugin = NULL;
      }
    }

    if (pPlugin) {
      iPair->second->push_back(pPlugin);
      pluginArray.push_back(pPlugin);
    }
  }
}
///////////////////////////////////////////////////////////////
void Plugins::List(ostream &o) const
{
  for (TypePluginsMap::const_iterator iPair = plugins.begin() ; iPair != plugins.end() ; iPair++) {
    if (iPair->second) {
      o << endl;
      o << "List of " << type2str(iPair->first) << " modules:" << endl;

      for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++)
        o << "  " << (*i)->Name() << " - " << (*i)->Description() << endl;
    }
  }
}
///////////////////////////////////////////////////////////////
void Plugins::Help(
    const char *pProgPath,
    const char *pPluginName) const
{
  BOOL found = FALSE;

  for (TypePluginsMap::const_iterator iPair = plugins.begin() ; iPair != plugins.end() ; iPair++) {
    if (iPair->second) {
      for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++) {
        if (*i && ((*i)->Name() == pPluginName || string("*") == pPluginName)) {
          if (found)
            cerr << "-----------------------------" << endl;

          cerr << "Type:        " << type2str(iPair->first) << endl;
          cerr << "Name:        " << (*i)->Name() << endl;
          cerr << "Copyright:   " << (*i)->Copyright() << endl;
          cerr << "License:     " << (*i)->License() << endl;
          cerr << "Description: " << (*i)->Description() << endl;
          cerr << endl;
          (*i)->Help(pProgPath);

          found = TRUE;
        }
      }
    }
  }

  if (!found)
    cerr << "The plugin " << pPluginName << " not found." << endl;
}
///////////////////////////////////////////////////////////////
BOOL Plugins::Config(const char *pArg) const
{
  BOOL res = FALSE;

  for (TypePluginsMap::const_iterator iPair = plugins.begin() ; iPair != plugins.end() ; iPair++) {
    if (iPair->second) {
      for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++) {
        if (*i && (*i)->Config(pArg))
          res = TRUE;
      }
    }
  }

  return res;
}
///////////////////////////////////////////////////////////////
const PLUGIN_ROUTINES_A *Plugins::GetRoutines(
    PLUGIN_TYPE type,
    const char *pPluginName,
    HCONFIG *phConfig) const
{
  TypePluginsMap::const_iterator iPair = plugins.find(type);

  if (iPair == plugins.end() || !iPair->second)
    return NULL;

  for (PluginArray::const_iterator i = iPair->second->begin() ; i != iPair->second->end() ; i++) {
    if (*i && (*i)->Name() == pPluginName)
      return (*i)->Routines(phConfig);
  }

  return NULL;
}
///////////////////////////////////////////////////////////////
