/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "interfaces/json-rpc/JSONRPC.h"

#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
JSONRPC_STATUS FailWithTarget(const CVariant& parameterObject, CVariant& result)
{
  result["partial"] = true;
  return Fail(result, Reason::NothingPlaying, Target("playlist", "audio"));
}

JSONRPC_STATUS FailWithoutTarget(const CVariant& parameterObject, CVariant& result)
{
  return Fail(result, Reason::Unreachable);
}

JSONRPC_STATUS FailWithStatusAlone(const CVariant& parameterObject, CVariant& result)
{
  result["partial"] = true;
  return FailedToExecute;
}
} // namespace

class TestJSONRPCErrorData : public JSONServiceDescriptionTestBase
{
protected:
  CVariant Respond(const std::string& name, JSONRPC::MethodCall::Handler handler)
  {
    EXPECT_TRUE(CJSONServiceDescription::AddMethod(std::string{R"({"Test.)"} + name + R"(": {
  "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
  "params": [], "returns": "string", "errors": []
}})",
                                                   handler));
    return ParseJson(CJSONRPC::MethodCall(std::string{R"({"jsonrpc": "2.0", "method": "Test.)"} +
                                              name + R"(", "id": 1})",
                                          &m_transport, &m_client));
  }
};

//! \brief A handler failing for a reason answers with the reason's status and the reason in data
TEST_F(TestJSONRPCErrorData, AReasonAndItsTargetReachErrorData)
{
  const CVariant response = Respond("FailWithTarget", FailWithTarget);

  EXPECT_EQ(FailedToExecute, response["error"]["code"].asInteger());
  const CVariant& data = response["error"]["data"];
  EXPECT_EQ("nothing-playing", data["reason"].asString());
  EXPECT_EQ("audio", data["target"]["playlist"].asString());
  EXPECT_FALSE(data.isMember("partial")) << ToJson(data);
}

TEST_F(TestJSONRPCErrorData, AReasonWithoutATargetOmitsIt)
{
  const CVariant response = Respond("FailWithoutTarget", FailWithoutTarget);

  EXPECT_EQ(Unavailable, response["error"]["code"].asInteger());
  EXPECT_EQ("unreachable", response["error"]["data"]["reason"].asString());
  EXPECT_FALSE(response["error"]["data"].isMember("target"));
}

//! \brief A status alone still fails the call, and what the handler had written stays private
TEST_F(TestJSONRPCErrorData, AStatusAloneCarriesNoData)
{
  const CVariant response = Respond("FailWithStatusAlone", FailWithStatusAlone);

  EXPECT_EQ(FailedToExecute, response["error"]["code"].asInteger());
  EXPECT_FALSE(response["error"].isMember("data")) << ToJson(response);
}

//! \brief The validator's data is unchanged by reasons
TEST_F(TestJSONRPCErrorData, TheValidatorStillDescribesInvalidParams)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(R"({"Test.Params": {
  "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
  "params": [{"name": "value", "required": true, "schema": {"type": "integer"}}],
  "returns": "string", "errors": []
}})",
                                                 StubMethod));

  const CVariant response = ParseJson(CJSONRPC::MethodCall(
      R"({"jsonrpc": "2.0", "method": "Test.Params", "params": {"value": "x"}, "id": 1})",
      &m_transport, &m_client));

  EXPECT_EQ(InvalidParams, response["error"]["code"].asInteger());
  EXPECT_EQ("Test.Params", response["error"]["data"]["method"].asString());
}
