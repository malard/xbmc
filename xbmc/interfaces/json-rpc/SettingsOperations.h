/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "JSONRPC.h"
#include "settings/lib/SettingLevel.h"

#include <vector>

class CVariant;
class ISetting;
class CSettingSection;
class CSettingCategory;
class CSettingGroup;
class CSetting;
class CSettingBool;
class CSettingInt;
class CSettingNumber;
class CSettingString;
class CSettingList;
class ISettingControl;

namespace JSONRPC
{
  class CSettingsOperations
  {
  public:
    static JSONRPC_STATUS GetLevel(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetLevel(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS GetSections(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetCategories(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetSettings(const CVariant &parameterObject, CVariant &result);

    static JSONRPC_STATUS GetSettingValue(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS SetSettingValue(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS ResetSettingValue(const CVariant &parameterObject, CVariant &result);

    static JSONRPC_STATUS GetSkinSettings(const CVariant& parameterObject, CVariant& result);
    static JSONRPC_STATUS GetSkinSettingValue(const CVariant& parameterObject, CVariant& result);
    static JSONRPC_STATUS SetSkinSettingValue(const CVariant& parameterObject, CVariant& result);

  private:
  static bool SerializeISetting(const std::shared_ptr<const ISetting>& setting, CVariant& obj);
    static bool SerializeSettingSection(const std::shared_ptr<const CSettingSection>& setting,
                                        CVariant& obj);
    static bool SerializeSettingCategory(const std::shared_ptr<const CSettingCategory>& setting,
                                         CVariant& obj);
    static bool SerializeSettingGroup(const std::shared_ptr<const CSettingGroup>& setting,
                                      CVariant& obj);
    static bool SerializeSetting(const std::shared_ptr<const CSetting>& setting, CVariant& obj);
    static void SerializeSettingBool(const std::shared_ptr<const CSettingBool>& setting,
                                     CVariant& obj);
    static void SerializeSettingInt(const std::shared_ptr<const CSettingInt>& setting,
                                    CVariant& obj);
    static void SerializeSettingNumber(const std::shared_ptr<const CSettingNumber>& setting,
                                       CVariant& obj);
    static void SerializeSettingString(const std::shared_ptr<const CSettingString>& setting,
                                       CVariant& obj);
    static bool SerializeSettingList(const std::shared_ptr<const CSettingList>& setting,
                                     CVariant& obj);
    static bool SerializeSettingControl(const std::shared_ptr<const ISettingControl>& control,
                                        CVariant& obj);

    static void SerializeSettingListValues(const std::vector<CVariant> &values, CVariant &obj);
  };
} // namespace JSONRPC
