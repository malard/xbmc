/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "utils/Variant.h"

#include <map>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

//! \brief The properties a type declares, including those of every type it extends
std::set<std::string> DeclaredProperties(const std::map<std::string, CVariant>& types,
                                         const std::string& name)
{
  constexpr std::string_view defsPointer{"#/$defs/"};

  const auto type{types.find(name)};
  if (type == types.end())
  {
    ADD_FAILURE() << name << " is not declared in the service description";
    return {};
  }

  std::set<std::string> properties{Keys(type->second["properties"])};

  const CVariant& bases{type->second["allOf"]};
  for (auto base = bases.begin_array(); base != bases.end_array(); ++base)
  {
    const std::string reference{(*base)["$ref"].asString()};
    if (reference.starts_with(defsPointer))
      properties.merge(DeclaredProperties(types, reference.substr(defsPointer.size())));
  }

  return properties;
}

} // unnamed namespace

/*!
 A field a caller may ask for that the details type does not declare arrives
 with no documented type or meaning.
 */
TEST(TestRequestableFields, EveryRequestableFieldIsDeclared)
{
  const std::map<std::string, CVariant> types{ShippedTypes()};

  for (const auto& [name, type] : types)
  {
    const size_t fields{name.find(".Fields.")};
    if (fields == std::string::npos)
      continue;

    std::string details{name};
    details.replace(fields, std::string_view{".Fields."}.size(), ".Details.");
    if (!types.contains(details))
      continue;

    const std::set<std::string> declared{DeclaredProperties(types, details)};
    for (const std::string& field : EnumValues(type["items"]))
    {
      EXPECT_TRUE(declared.contains(field))
          << name << " offers \"" << field << "\", which " << details << " does not declare";
    }
  }
}
