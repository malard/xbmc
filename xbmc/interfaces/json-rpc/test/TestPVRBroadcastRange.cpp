/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "XBDateTime.h"
#include "interfaces/json-rpc/JSONRPCUtils.h"
#include "interfaces/json-rpc/PVROperations.h"
#include "utils/Variant.h"

#include <map>
#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

class CTestPVROperations : public CPVROperations
{
public:
  static JSONRPC_STATUS Parse(const CVariant& parameterObject,
                              bool required,
                              CDateTime& start,
                              CDateTime& end)
  {
    return ParseTimeRange(parameterObject, required, start, end);
  }
};

CVariant Request(const std::string& starttime, const std::string& endtime)
{
  CVariant params{CVariant::VariantTypeObject};
  // the service description fills every declared parameter before a handler runs, so an
  // omitted time arrives as an empty string rather than as an absent member
  params["startTime"] = starttime;
  params["endTime"] = endtime;
  return params;
}

} // unnamed namespace

/*!
 The range on PVR.GetBroadcasts is an addition to a method clients already call, so a
 request that does not give one must keep meaning "everything".
 */
TEST(TestPVRBroadcastRange, TheRangeOnGetBroadcastsIsOptional)
{
  const std::map<std::string, CVariant> params{Params(ShippedMethod("PVR.GetBroadcasts"))};

  ASSERT_TRUE(params.contains("startTime"));
  ASSERT_TRUE(params.contains("endTime"));
  EXPECT_FALSE(params.at("startTime")["required"].asBoolean());
  EXPECT_FALSE(params.at("endTime")["required"].asBoolean());
}

/*!
 The group form always needs a range; a limit over channels has no meaning, so there is none.
 */
TEST(TestPVRBroadcastRange, TheGroupFormRequiresARange)
{
  const std::map<std::string, CVariant> params{
      Params(ShippedMethod("PVR.GetBroadcastsByChannelGroup"))};

  ASSERT_TRUE(params.contains("channelGroupId"));
  ASSERT_TRUE(params.contains("startTime"));
  ASSERT_TRUE(params.contains("endTime"));
  EXPECT_TRUE(params.at("channelGroupId")["required"].asBoolean());
  EXPECT_TRUE(params.at("startTime")["required"].asBoolean());
  EXPECT_TRUE(params.at("endTime")["required"].asBoolean());
  EXPECT_TRUE(params.contains("properties"));
  EXPECT_FALSE(params.contains("limits"));
}

/*!
 A broadcast does not carry the Kodi channel id it belongs to, so the group form has to
 answer per channel for the caller to tell them apart.
 */
TEST(TestPVRBroadcastRange, TheGroupFormAnswersPerChannel)
{
  const CVariant returns{ShippedMethod("PVR.GetBroadcastsByChannelGroup")["returns"]};

  EXPECT_TRUE(RequiredMembers(returns).contains("channels"));

  const CVariant& channel{returns["properties"]["channels"]["items"]};
  const std::set<std::string> required{RequiredMembers(channel)};
  EXPECT_TRUE(required.contains("channelId"));
  EXPECT_TRUE(required.contains("broadcasts"));
  EXPECT_EQ(channel["properties"]["broadcasts"]["items"]["$ref"].asString(),
            "#/$defs/PVR.Details.Broadcast");
}

TEST(TestPVRBroadcastRange, AnOmittedOptionalRangeMeansNoRange)
{
  CDateTime start;
  CDateTime end;
  EXPECT_EQ(CTestPVROperations::Parse(Request("", ""), false, start, end), OK);
  EXPECT_FALSE(start.IsValid());
  EXPECT_FALSE(end.IsValid());
}

TEST(TestPVRBroadcastRange, AnOmittedRequiredRangeIsRejected)
{
  CDateTime start;
  CDateTime end;
  EXPECT_EQ(CTestPVROperations::Parse(Request("", ""), true, start, end), InvalidParams);
}

TEST(TestPVRBroadcastRange, HalfARangeIsRejected)
{
  CDateTime start;
  CDateTime end;
  EXPECT_EQ(CTestPVROperations::Parse(Request("2026-08-19 10:00:00", ""), false, start, end),
            InvalidParams);
  EXPECT_EQ(CTestPVROperations::Parse(Request("", "2026-08-19 12:00:00"), false, start, end),
            InvalidParams);
}

TEST(TestPVRBroadcastRange, AnInvertedRangeIsRejected)
{
  CDateTime start;
  CDateTime end;
  EXPECT_EQ(CTestPVROperations::Parse(Request("2026-08-19 12:00:00", "2026-08-19 10:00:00"), false,
                                      start, end),
            InvalidParams);
}

TEST(TestPVRBroadcastRange, ARangeIsReadInTheBroadcastTimeFormat)
{
  CDateTime start;
  CDateTime end;
  ASSERT_EQ(CTestPVROperations::Parse(Request("2026-08-19 10:00:00", "2026-08-19 12:00:00"), false,
                                      start, end),
            OK);
  EXPECT_EQ(start.GetAsDBDateTime(), "2026-08-19 10:00:00");
  EXPECT_EQ(end.GetAsDBDateTime(), "2026-08-19 12:00:00");
}
