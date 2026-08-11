/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

namespace
{

/*!
 \brief How each type declaring a property declares it, keyed by type name

 Read out of the shipped service description rather than restated here, so a
 declaration that never reaches the schema fails instead of passing.
 */
std::map<std::string, std::string> DeclarationsOf(const std::string& property)
{
  std::map<std::string, std::string> declarations;

  for (const char* const entry : JSONRPC::JSONRPC_SERVICE_TYPES)
  {
    // Each entry is one definition without its enclosing braces
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entry) + "}", parsed))
    {
      continue;
    }

    for (auto type = parsed.begin_map(); type != parsed.end_map(); ++type)
    {
      const CVariant& properties{type->second["properties"]};
      if (!properties.isMember(property))
      {
        continue;
      }

      // Two types may describe the same value differently in prose
      CVariant declaration{properties[property]};
      declaration.erase("description");

      declarations.emplace(type->first, JSONRPC::ToJson(declaration));
    }
  }

  return declarations;
}

/*!
 A property carried by several types is one value with one meaning, so every
 type carrying it has to describe the same shape. A type that disagrees
 describes a value nothing sends, and a client generated from it branches on a
 type it will never receive.
 */
void ExpectOneDeclaration(const std::string& property)
{
  const std::map<std::string, std::string> declarations{DeclarationsOf(property)};
  ASSERT_GT(declarations.size(), 1U) << "\"" << property << "\" is carried by fewer than two types";

  const auto& [firstType, firstDeclaration] = *declarations.begin();
  for (const auto& [type, declaration] : declarations)
  {
    EXPECT_EQ(firstDeclaration, declaration)
        << type << " declares \"" << property << "\" as " << declaration << ", while " << firstType
        << " declares it as " << firstDeclaration;
  }
}

} // unnamed namespace

TEST(TestSharedFieldDeclarations, GenreIsTheSameValueWhereverItIsCarried)
{
  ExpectOneDeclaration("genre");
}

TEST(TestSharedFieldDeclarations, ImdbNumberIsTheSameValueWhereverItIsCarried)
{
  ExpectOneDeclaration("imdbnumber");
}
