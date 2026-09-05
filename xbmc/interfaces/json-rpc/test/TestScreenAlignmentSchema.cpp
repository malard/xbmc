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

#include <gtest/gtest.h>

using namespace JSONRPC;

//! \brief A caller reading the state back gets the same shape it was answered when it set it
TEST(TestScreenAlignmentSchema, BothMethodsAnswerTheSameType)
{
  EXPECT_EQ("GUI.ScreenAlignment",
            ShippedMethod("GUI.SetScreenAlignment")["returns"]["$ref"].asString());
  EXPECT_EQ("GUI.ScreenAlignment",
            ShippedMethod("GUI.GetScreenAlignment")["returns"]["$ref"].asString());
}

//! \brief Every field is required, so a client never has to distinguish absent from empty
TEST(TestScreenAlignmentSchema, TheStateIsFullyPopulated)
{
  const CVariant type = ShippedType("GUI.ScreenAlignment");

  EXPECT_EQ(std::set<std::string>({"showing", "ratios", "available"}), Keys(type["properties"]));

  for (const std::string& name : {"showing", "ratios", "available"})
    EXPECT_TRUE(type["properties"][name]["required"].asBoolean()) << name << " is not required";
}

/*!
 Both arrays carry ratios, and PublishedAspect() rounds them to four places so that a caller can
 compare what it asked for against what it was given. Declaring them as strings, or as an enum of
 labels, would put that comparison beyond a client that only knows the number.
 */
TEST(TestScreenAlignmentSchema, RatiosAreNumbers)
{
  const CVariant type = ShippedType("GUI.ScreenAlignment");

  for (const std::string& name : {"ratios", "available"})
  {
    EXPECT_EQ("array", type["properties"][name]["type"].asString()) << name;
    EXPECT_EQ("number", type["properties"][name]["items"]["type"].asString()) << name;
    EXPECT_TRUE(type["properties"][name]["uniqueItems"].asBoolean()) << name << " permits repeats";
  }
}

//! \brief Neither parameter is required: a set may be stated without showing, and the reverse
TEST(TestScreenAlignmentSchema, SettingEitherHalfAloneIsAllowed)
{
  const CVariant method = ShippedMethod("GUI.SetScreenAlignment");

  for (const std::string& name : {"show", "ratios"})
  {
    const CVariant* const param = Param(method, name);
    ASSERT_NE(nullptr, param) << name << " is not declared";
    EXPECT_FALSE((*param)["required"].asBoolean()) << name << " is required";
  }
}

/*!
 An omitted parameter is materialised as its type's empty value before the handler runs, so an
 absent boolean arrives as false and an absent array as the empty one. Both are meaningful here -
 false hides the tool, and the empty array clears every frame - so each parameter has to admit
 null as well, which is then the only value that means the caller stated nothing.
 */
TEST(TestScreenAlignmentSchema, AnUnstatedParameterIsDistinguishableFromAStatedEmptyOne)
{
  const CVariant method = ShippedMethod("GUI.SetScreenAlignment");

  for (const std::string& name : {"show", "ratios"})
  {
    const CVariant* const param = Param(method, name);
    ASSERT_NE(nullptr, param) << name << " is not declared";

    const CVariant& schema = (*param)["schema"];
    EXPECT_TRUE(schema["default"].isNull()) << name << " does not default to null";

    bool nullable = false;
    const CVariant& alternatives = schema["anyOf"];
    for (auto it = alternatives.begin_array(); it != alternatives.end_array(); ++it)
      nullable = nullable || (*it)["type"].asString() == "null";

    EXPECT_TRUE(nullable) << name << " cannot be stated as null";
  }
}

//! \brief Showing frames is a control operation; reading which are shown is not
TEST(TestScreenAlignmentSchema, TheWriteIsGatedMoreTightlyThanTheRead)
{
  EXPECT_EQ("ControlGUI", ShippedMethod("GUI.SetScreenAlignment")["permission"].asString());
  EXPECT_EQ("ReadData", ShippedMethod("GUI.GetScreenAlignment")["permission"].asString());
}

/*!
 The window is created with the window manager and destroyed with it, so a call arriving before
 the GUI exists or after it has gone answers FailedToExecute rather than reporting no frames.
 */
TEST(TestScreenAlignmentSchema, BothMethodsDeclareTheMissingWindow)
{
  for (const std::string& name : {"GUI.SetScreenAlignment", "GUI.GetScreenAlignment"})
  {
    const CVariant errors = ShippedMethod(name)["errors"];
    bool declared = false;
    for (auto error = errors.begin_array(); error != errors.end_array(); ++error)
      declared = declared || error->asString() == "FailedToExecute";

    EXPECT_TRUE(declared) << name << " does not declare FailedToExecute";
  }
}
