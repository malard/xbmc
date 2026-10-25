/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/JSONRPCUtils.h"

#include <array>
#include <set>
#include <string_view>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
//! Every JSONRPC_STATUS that reaches a client as an error. OK and ACK produce a result.
constexpr std::array<JSONRPC_STATUS, 10> ERROR_STATUSES{
    ParseError,      InvalidRequest, MethodNotFound, InvalidParams, InternalError,
    FailedToExecute, BadPermission,  NotFound,       Unavailable,   AccessDenied};
} // namespace

//! \brief A status added to JSONRPC_STATUS must also be described, or clients cannot discover it
TEST(TestJSONRPCStatus, EveryErrorStatusIsDescribed)
{
  for (const auto status : ERROR_STATUSES)
    EXPECT_NE(nullptr, StatusToDescription(status))
        << "undescribed status " << static_cast<int>(status);

  EXPECT_EQ(ERROR_STATUSES.size(), JSONRPC_STATUS_DESCRIPTIONS.size());
}

TEST(TestJSONRPCStatus, SuccessStatusesAreNotDescribed)
{
  EXPECT_EQ(nullptr, StatusToDescription(OK));
  EXPECT_EQ(nullptr, StatusToDescription(ACK));
}

TEST(TestJSONRPCStatus, CodesAndNamesAreUnique)
{
  std::set<int> codes;
  std::set<std::string_view> names;

  for (const auto& description : JSONRPC_STATUS_DESCRIPTIONS)
  {
    EXPECT_TRUE(codes.insert(static_cast<int>(description.status)).second)
        << "duplicate code " << static_cast<int>(description.status);
    EXPECT_TRUE(names.insert(description.name).second) << "duplicate name " << description.name;
  }
}

//! \brief Only InvalidParams populates "error.data" without a reason; the validator writes it
TEST(TestJSONRPCStatus, OnlyInvalidParamsCarriesData)
{
  for (const auto& description : JSONRPC_STATUS_DESCRIPTIONS)
    EXPECT_EQ(description.status == InvalidParams, description.hasData) << description.name;
}

TEST(TestJSONRPCStatus, EveryDescriptionIsPopulated)
{
  for (const auto& description : JSONRPC_STATUS_DESCRIPTIONS)
  {
    EXPECT_FALSE(std::string_view(description.name).empty());
    EXPECT_FALSE(std::string_view(description.message).empty());
    EXPECT_FALSE(std::string_view(description.description).empty());
  }
}

TEST(TestJSONRPCStatus, ReasonNamesAreUniqueAndKebabCase)
{
  std::set<std::string_view> names;

  for (const auto& description : JSONRPC_REASON_DESCRIPTIONS)
  {
    const std::string_view name{description.name};
    EXPECT_TRUE(names.insert(name).second) << "duplicate reason " << name;
    EXPECT_FALSE(name.empty());
    EXPECT_EQ(std::string_view::npos, name.find_first_not_of("abcdefghijklmnopqrstuvwxyz-"))
        << name;
    EXPECT_NE('-', name.front()) << name;
    EXPECT_NE('-', name.back()) << name;
    EXPECT_EQ(std::string_view::npos, name.find("--")) << name;
  }
}

TEST(TestJSONRPCStatus, EveryReasonIsDescribed)
{
  for (const auto& description : JSONRPC_REASON_DESCRIPTIONS)
  {
    EXPECT_EQ(&description, &ReasonToDescription(description.reason)) << description.name;
    EXPECT_FALSE(std::string_view(description.description).empty()) << description.name;
  }
}
