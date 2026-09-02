/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceBroker.h"
#include "ServiceDescription.h"
#include "commons/ilog.h"
#include "interfaces/json-rpc/ApplicationOperations.h"
#include "interfaces/json-rpc/JSONRPCUtils.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <map>
#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

class CTestApplicationOperations : public CApplicationOperations
{
public:
  static std::string Name(int level) { return LogLevelName(level); }
  static int FromName(const std::string& name) { return LogLevelFromName(name); }
};

//! \brief A definition out of the shipped service description, by name
CVariant Definition(const char* const entries[], size_t count, const std::string& name)
{
  for (size_t index = 0; index < count; ++index)
  {
    // Each entry is one definition without its enclosing braces
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entries[index]) + "}", parsed))
      continue;

    if (parsed.isMember(name))
      return parsed[name];
  }

  ADD_FAILURE() << name << " is not declared in the service description";
  return {};
}

CVariant Type(const std::string& name)
{
  return Definition(JSONRPC_SERVICE_TYPES, std::size(JSONRPC_SERVICE_TYPES), name);
}

CVariant Method(const std::string& name)
{
  return Definition(JSONRPC_SERVICE_METHODS, std::size(JSONRPC_SERVICE_METHODS), name);
}

std::set<std::string> Enum(const CVariant& type)
{
  std::set<std::string> values;
  const CVariant& list{type["enum"]};
  for (auto value = list.begin_array(); value != list.end_array(); ++value)
    values.insert(value->asString());
  return values;
}

std::map<std::string, CVariant> Params(const CVariant& method)
{
  std::map<std::string, CVariant> params;
  const CVariant& values{method["params"]};
  for (auto value = values.begin_array(); value != values.end_array(); ++value)
    params.emplace((*value)["name"].asString(), *value);
  return params;
}

/*!
 \brief Puts the log level back when a test leaves scope, so the suite's own logging is unaffected
 */
class CLogLevelGuard
{
public:
  CLogLevelGuard()
    : m_level{CServiceBroker::GetLogging().GetLogLevel()},
      m_advancedLevel{CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_logLevel},
      m_toggle{CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
          CSettings::SETTING_DEBUG_SHOWLOGINFO)}
  {
  }
  ~CLogLevelGuard()
  {
    // the toggle first: its callback moves the level, and the level is restored after it
    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(
        CSettings::SETTING_DEBUG_SHOWLOGINFO, m_toggle);
    CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_logLevel = m_advancedLevel;
    CServiceBroker::GetLogging().SetLogLevel(m_level);
  }

private:
  int m_level;
  int m_advancedLevel;
  bool m_toggle;
};

} // unnamed namespace

/*!
 The level is reported as a property of the application, next to volume and mute, and
 moved by a setter next to theirs.
 */
TEST(TestApplicationLogLevel, TheLevelIsAnApplicationProperty)
{
  EXPECT_TRUE(Enum(Type("Application.Property.Name")).contains("loglevel"));
  EXPECT_EQ(Type("Application.Property.Value")["properties"]["loglevel"]["$ref"].asString(),
            "#/$defs/Application.LogLevel.Value");
}

/*!
 Both parameters are optional and default to null, because the service description fills
 every omitted parameter before the handler runs: null is the only value that can mean
 "leave it as it is".
 */
TEST(TestApplicationLogLevel, TheSetterLeavesWhatItIsNotGiven)
{
  const CVariant method{Method("Application.SetLogLevel")};
  EXPECT_EQ(method["permission"].asString(), "ControlSystem");

  const std::map<std::string, CVariant> params{Params(method)};
  ASSERT_TRUE(params.contains("level"));
  ASSERT_TRUE(params.contains("components"));
  EXPECT_FALSE(params.at("level")["required"].asBoolean());
  EXPECT_FALSE(params.at("components")["required"].asBoolean());
  EXPECT_TRUE(params.at("level")["schema"]["default"].isNull());
  EXPECT_TRUE(params.at("components")["schema"]["default"].isNull());
  EXPECT_EQ(method["returns"]["$ref"].asString(), "#/$defs/Application.LogLevel.Value");
}

/*!
 Every name the schema offers is one the handler understands, and every level the handler
 can report is one the schema declares - so the two cannot drift apart silently.
 */
TEST(TestApplicationLogLevel, TheSchemaAndTheHandlerAgreeOnTheNames)
{
  const std::set<std::string> names{Enum(Type("Application.LogLevel"))};
  ASSERT_EQ(names.size(), 4u);

  for (const std::string& name : names)
  {
    const int level{CTestApplicationOperations::FromName(name)};
    EXPECT_GE(level, LOG_LEVEL_NONE) << name << " is in the schema and unknown to the handler";
    EXPECT_EQ(CTestApplicationOperations::Name(level), name);
  }

  for (int level = LOG_LEVEL_NONE; level <= LOG_LEVEL_MAX; ++level)
  {
    EXPECT_TRUE(names.contains(CTestApplicationOperations::Name(level)))
        << "level " << level << " has no name in the schema";
  }

  EXPECT_TRUE(CTestApplicationOperations::Name(LOG_LEVEL_MAX + 1).empty());
  EXPECT_LT(CTestApplicationOperations::FromName("verbose"), LOG_LEVEL_NONE);
}

TEST(TestApplicationLogLevel, AnUnknownComponentIsRejected)
{
  CVariant params{CVariant::VariantTypeObject};
  params["level"] = CVariant{};
  params["components"] = CVariant{CVariant::VariantTypeArray};
  params["components"].append("jsonrpc");
  params["components"].append("no-such-component");

  CVariant result;
  EXPECT_EQ(CApplicationOperations::SetLogLevel("", nullptr, nullptr, params, result),
            InvalidParams);
}

/*!
 The answer is what is now in force, read back from the logger rather than echoed from
 the request.
 */
TEST(TestApplicationLogLevel, TheSetterAnswersWithWhatIsInForce)
{
  CLogLevelGuard guard;

  CVariant params{CVariant::VariantTypeObject};
  params["level"] = "normal";
  params["components"] = CVariant{};

  CVariant result;
  ASSERT_EQ(CApplicationOperations::SetLogLevel("", nullptr, nullptr, params, result), OK);
  EXPECT_EQ(result["level"].asString(), "normal");
  EXPECT_EQ(CServiceBroker::GetLogging().GetLogLevel(), LOG_LEVEL_NORMAL);

  params["level"] = "debug";
  ASSERT_EQ(CApplicationOperations::SetLogLevel("", nullptr, nullptr, params, result), OK);
  EXPECT_EQ(result["level"].asString(), "debug");
  EXPECT_EQ(CServiceBroker::GetLogging().GetLogLevel(), LOG_LEVEL_DEBUG);

  // every component this build knows is listed, by its stable name
  std::set<std::string> listed;
  for (auto it = result["components"].begin_array(); it != result["components"].end_array(); ++it)
  {
    listed.insert((*it)["name"].asString());
    EXPECT_TRUE((*it)["enabled"].isBoolean());
  }
  for (const std::string& name : CLog::GetComponentNames())
    EXPECT_TRUE(listed.contains(name)) << name;
}
