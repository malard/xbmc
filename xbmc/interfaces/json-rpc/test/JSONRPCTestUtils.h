/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ServiceDescription.h"
#include "interfaces/json-rpc/IClient.h"
#include "interfaces/json-rpc/ITransportLayer.h"
#include "interfaces/json-rpc/JSONRPCUtils.h"
#include "interfaces/json-rpc/JSONServiceDescription.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <string>

#include <gtest/gtest.h>

namespace JSONRPC
{

class CAllCapabilityTransport : public ITransportLayer
{
public:
  bool PrepareDownload(const char* path, CVariant& details, std::string& protocol) override
  {
    return false;
  }
  bool Download(const char* path, CVariant& result) override { return false; }
  int GetCapabilities() override { return TRANSPORT_LAYER_CAPABILITY_ALL; }
};

class CAllPermissionClient : public IClient
{
public:
  int GetPermissionFlags() override { return OPERATION_PERMISSION_ALL; }
  int GetAnnouncementFlags() override { return 0; }
  bool SetAnnouncementFlags(int flags) override { return true; }
};

inline JSONRPC_STATUS StubMethod(const std::string& method,
                                 ITransportLayer* transport,
                                 IClient* client,
                                 const CVariant& parameterObject,
                                 CVariant& result)
{
  return OK;
}

inline CVariant ParseJson(const std::string& json)
{
  CVariant parsed;
  EXPECT_TRUE(CJSONVariantParser::Parse(json, parsed)) << "invalid test JSON: " << json;
  return parsed;
}

inline std::string ToJson(const CVariant& variant)
{
  std::string json;
  CJSONVariantWriter::Write(variant, json, true);
  return json;
}

/*!
 \brief One type's entry, as the generated service description ships it

 Read rather than restated, so that a declaration which never reaches the schema fails here
 instead of passing. The result is ready to hand to CJSONServiceDescription::AddType.
 */
inline std::string ShippedDefinition(const std::string& type)
{
  for (const char* const entry : JSONRPC_SERVICE_TYPES)
  {
    // Each entry is one definition without its enclosing braces
    const std::string definition{"{" + std::string(entry) + "}"};

    CVariant parsed;
    if (CJSONVariantParser::Parse(definition, parsed) && parsed.isMember(type))
    {
      return definition;
    }
  }

  ADD_FAILURE() << type << " is not declared in the service description";
  return {};
}

/*!
 \brief Fixture isolating the global schema registry of CJSONServiceDescription.
 */
class JSONServiceDescriptionTestBase : public ::testing::Test
{
public:
  void SetUp() override { CJSONServiceDescription::Cleanup(); }
  void TearDown() override { CJSONServiceDescription::Cleanup(); }

  JSONRPC_STATUS Call(const char* method, const std::string& paramsJson, CVariant& output)
  {
    // the transport layer lowercases the method before dispatch
    std::string key = method;
    StringUtils::ToLower(key);
    MethodCall call = nullptr;
    output = CVariant();
    return CJSONServiceDescription::CheckCall(key.c_str(), ParseJson(paramsJson), &m_transport,
                                              &m_client, false, call, output);
  }

  CAllCapabilityTransport m_transport;
  CAllPermissionClient m_client;
};

} // namespace JSONRPC
