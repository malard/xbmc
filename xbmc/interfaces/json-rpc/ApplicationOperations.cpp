/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ApplicationOperations.h"

#include "CompileInfo.h"
#include "ServiceBroker.h"
#include "language/Language.h"
#include "messaging/ApplicationMessenger.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

using namespace JSONRPC;

JSONRPC_STATUS CApplicationOperations::GetProperties(const CVariant& parameterObject,
                                                     CVariant& result)
{
  return GetNamedProperties(parameterObject, result, GetPropertyValue);
}

namespace
{
constexpr std::array<std::pair<int, const char*>, 4> LOG_LEVEL_NAMES{{
    {LOG_LEVEL_NONE, "none"},
    {LOG_LEVEL_NORMAL, "normal"},
    {LOG_LEVEL_DEBUG, "debug"},
    {LOG_LEVEL_DEBUG_FREEMEM, "debugFreeMem"},
}};
} // unnamed namespace

JSONRPC_STATUS CApplicationOperations::SetLogLevel(const CVariant& parameterObject,
                                                   CVariant& result)
{
  const CVariant& levelParam{parameterObject["level"]};
  const CVariant& componentsParam{parameterObject["components"]};

  std::optional<int> level;
  if (!levelParam.isNull())
  {
    level = LogLevelFromName(levelParam.asString());
    if (!level)
      return InvalidParams;
  }

  std::vector<CVariant> componentIds;
  if (!componentsParam.isNull())
  {
    for (auto it = componentsParam.begin_array(); it != componentsParam.end_array(); ++it)
    {
      const uint32_t id{CLog::GetComponentByName(it->asString())};
      if (id == CLog::LOG_COMPONENT_GENERAL)
        return InvalidParams;
      componentIds.emplace_back(static_cast<int>(id));
    }
  }

  const auto settings{CServiceBroker::GetSettingsComponent()->GetSettings()};

  if (level)
  {
    // SetDebugMode cannot express none or debugfreemem, so the exact level is applied after it.
    settings->SetBool(CSettings::SETTING_DEBUG_SHOWLOGINFO, *level >= LOG_LEVEL_DEBUG);
    CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->SetLogLevel(*level);
  }

  if (!componentsParam.isNull())
  {
    // CLog listens to both settings, so there is nothing to tell it directly
    settings->SetList(CSettings::SETTING_DEBUG_SETEXTRALOGLEVEL, componentIds);
    settings->SetBool(CSettings::SETTING_DEBUG_EXTRALOGGING, !componentIds.empty());
  }

  result = LogLevelValue();
  return OK;
}

std::string CApplicationOperations::LogLevelName(int level)
{
  for (const auto& [value, name] : LOG_LEVEL_NAMES)
  {
    if (value == level)
      return name;
  }
  return {};
}

std::optional<int> CApplicationOperations::LogLevelFromName(const std::string& name)
{
  for (const auto& [value, levelName] : LOG_LEVEL_NAMES)
  {
    if (name == levelName)
      return value;
  }
  return std::nullopt;
}

CVariant CApplicationOperations::LogLevelValue()
{
  CVariant value{CVariant::VariantTypeObject};
  value["level"] = LogLevelName(CServiceBroker::GetLogging().GetLogLevel());

  value["components"] = CVariant{CVariant::VariantTypeArray};
  for (const std::string& name : CLog::GetComponentNames())
  {
    CVariant component{CVariant::VariantTypeObject};
    component["name"] = name;
    component["enabled"] =
        CServiceBroker::GetLogging().CanLogComponent(CLog::GetComponentByName(name));
    value["components"].append(std::move(component));
  }
  return value;
}

JSONRPC_STATUS CApplicationOperations::Quit(const CVariant& parameterObject, CVariant& result)
{
  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_QUIT);
  return ACK;
}

JSONRPC_STATUS CApplicationOperations::GetPropertyValue(const std::string& property,
                                                        CVariant& result)
{
  if (property == "name")
    result = CCompileInfo::GetAppName();
  else if (property == "version")
  {
    result = CVariant(CVariant::VariantTypeObject);
    result["major"] = CCompileInfo::GetMajor();
    result["minor"] = CCompileInfo::GetMinor();
    result["revision"] = CCompileInfo::GetSCMID();
    std::string tag = CCompileInfo::GetSuffix();
    if (StringUtils::StartsWithNoCase(tag, "alpha"))
    {
      result["tag"] = "alpha";
      result["tagVersion"] = StringUtils::Mid(tag, 5);
    }
    else if (StringUtils::StartsWithNoCase(tag, "beta"))
    {
      result["tag"] = "beta";
      result["tagVersion"] = StringUtils::Mid(tag, 4);
    }
    else if (StringUtils::StartsWithNoCase(tag, "rc"))
    {
      result["tag"] = "releaseCandidate";
      result["tagVersion"] = StringUtils::Mid(tag, 2);
    }
    else if (tag.empty())
      result["tag"] = "stable";
    else
      result["tag"] = "prealpha";
  }
  else if (property == "sortTokens")
  {
    result = CVariant(CVariant::VariantTypeArray); // Ensure no tokens returns as []
    const auto& sortTokens = KODI::LANGUAGE::CLanguage::GetInstance().SortTokens();
    for (const auto& token : sortTokens)
      result.append(token);
  }
  else if (property == "language")
  {
    result = KODI::LANGUAGE::CLanguage::GetInstance().UI().ToString();
  }
  else if (property == "logLevel")
    result = LogLevelValue();
  else
    return InvalidParams;

  return OK;
}
