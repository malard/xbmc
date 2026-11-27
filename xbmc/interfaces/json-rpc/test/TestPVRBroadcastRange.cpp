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
