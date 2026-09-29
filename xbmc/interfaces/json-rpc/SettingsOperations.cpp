/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SettingsOperations.h"

#include "GUIPassword.h"
#include "ServiceBroker.h"
#include "addons/Addon.h"
#include "addons/Skin.h"
#include "addons/addoninfo/AddonInfo.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/DisplaySettings.h"
#include "settings/SettingAddon.h"
#include "settings/SettingControl.h"
#include "settings/SettingPath.h"
#include "settings/SettingUtils.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/SkinSettings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingDefinitions.h"
#include "settings/lib/SettingSection.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "view/ViewStateSettings.h"

#include <algorithm>
#include <optional>

using namespace JSONRPC;

JSONRPC_STATUS CSettingsOperations::GetLevel(const CVariant& parameterObject, CVariant& result)
{
  result["level"] = SettingLevelToString(CViewStateSettings::GetInstance().GetSettingLevel());

  return OK;
}

JSONRPC_STATUS CSettingsOperations::SetLevel(const CVariant& parameterObject, CVariant& result)
{
  const SettingLevel level = ParseSettingLevel(parameterObject["level"].asString());
  CViewStateSettings& viewStateSettings = CViewStateSettings::GetInstance();

  if (level != viewStateSettings.GetSettingLevel())
  {
    if (!g_passwordManager.IsSettingLevelUnlocked(level))
      return Fail(result, Reason::LevelLocked, Target("level", parameterObject["level"]));

    viewStateSettings.SetSettingLevel(level);
    CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
  }

  result["level"] = SettingLevelToString(viewStateSettings.GetSettingLevel());

  return OK;
}

JSONRPC_STATUS CSettingsOperations::GetSections(const CVariant& parameterObject, CVariant& result)
{
  SettingLevel level = ParseSettingLevel(parameterObject["level"].asString());
  bool listCategories = !parameterObject["properties"].empty() &&
                        parameterObject["properties"][0].asString() == "categories";

  result["level"] = SettingLevelToString(level);
  result["sections"] = CVariant(CVariant::VariantTypeArray);

  // apply the level filter
  SettingSectionList allSections =
      CServiceBroker::GetSettingsComponent()->GetSettings()->GetSections();
  for (const auto& itSection : allSections)
  {
    SettingCategoryList categories = itSection->GetCategories(level);
    if (categories.empty())
      continue;

    CVariant varSection(CVariant::VariantTypeObject);
    if (!SerializeSettingSection(itSection, varSection))
      continue;

    if (listCategories)
    {
      varSection["categories"] = CVariant(CVariant::VariantTypeArray);
      for (const auto& itCategory : categories)
      {
        CVariant varCategory(CVariant::VariantTypeObject);
        if (!SerializeSettingCategory(itCategory, varCategory))
          continue;

        varSection["categories"].push_back(varCategory);
      }
    }

    result["sections"].push_back(varSection);
  }

  return OK;
}

JSONRPC_STATUS CSettingsOperations::GetCategories(const CVariant& parameterObject, CVariant& result)
{
  SettingLevel level = ParseSettingLevel(parameterObject["level"].asString());
  std::string strSection = parameterObject["section"].asString();
  bool listSettings = !parameterObject["properties"].empty() &&
                      parameterObject["properties"][0].asString() == "settings";

  std::vector<SettingSectionPtr> sections;
  if (!strSection.empty())
  {
    SettingSectionPtr section =
        CServiceBroker::GetSettingsComponent()->GetSettings()->GetSection(strSection);
    if (section == nullptr)
      return InvalidParams;

    sections.push_back(section);
  }
  else
    sections = CServiceBroker::GetSettingsComponent()->GetSettings()->GetSections();

  result["level"] = SettingLevelToString(level);
  result["categories"] = CVariant(CVariant::VariantTypeArray);

  for (const auto& itSection : sections)
  {
    SettingCategoryList categories = itSection->GetCategories(level);
    for (const auto& itCategory : categories)
    {
      CVariant varCategory(CVariant::VariantTypeObject);
      if (!SerializeSettingCategory(itCategory, varCategory))
        continue;

      if (listSettings)
      {
        varCategory["groups"] = CVariant(CVariant::VariantTypeArray);

        SettingGroupList groups = itCategory->GetGroups(level);
        for (const auto& itGroup : groups)
        {
          CVariant varGroup(CVariant::VariantTypeObject);
          if (!SerializeSettingGroup(itGroup, varGroup))
            continue;

          varGroup["settings"] = CVariant(CVariant::VariantTypeArray);
          SettingList settings = itGroup->GetSettings(level);
          for (const auto& itSetting : settings)
          {
            if (itSetting->IsVisible())
            {
              CVariant varSetting(CVariant::VariantTypeObject);
              if (!SerializeSetting(itSetting, varSetting))
                continue;

              varGroup["settings"].push_back(varSetting);
            }
          }

          varCategory["groups"].push_back(varGroup);
        }
      }

      result["categories"].push_back(varCategory);
    }
  }

  return OK;
}

JSONRPC_STATUS CSettingsOperations::GetSettings(const CVariant& parameterObject, CVariant& result)
{
  SettingLevel level = ParseSettingLevel(parameterObject["level"].asString());
  const CVariant& filter = parameterObject["filter"];
  bool doFilter = filter.isMember("section") && filter.isMember("category");
  std::string strSection, strCategory;
  if (doFilter)
  {
    strSection = filter["section"].asString();
    strCategory = filter["category"].asString();
  }

  std::vector<SettingSectionPtr> sections;

  if (doFilter)
  {
    SettingSectionPtr section =
        CServiceBroker::GetSettingsComponent()->GetSettings()->GetSection(strSection);
    if (section == nullptr)
      return InvalidParams;

    sections.push_back(section);
  }
  else
    sections = CServiceBroker::GetSettingsComponent()->GetSettings()->GetSections();

  result["level"] = SettingLevelToString(level);
  result["settings"] = CVariant(CVariant::VariantTypeArray);

  for (const auto& itSection : sections)
  {
    SettingCategoryList categories = itSection->GetCategories(level);
    bool found = !doFilter;
    for (const auto& itCategory : categories)
    {
      if (!doFilter || StringUtils::EqualsNoCase(itCategory->GetId(), strCategory))
      {
        SettingGroupList groups = itCategory->GetGroups(level);
        for (const auto& itGroup : groups)
        {
          SettingList settings = itGroup->GetSettings(level);
          for (const auto& itSetting : settings)
          {
            if (itSetting->IsVisible())
            {
              CVariant varSetting(CVariant::VariantTypeObject);
              if (!SerializeSetting(itSetting, varSetting))
                continue;

              result["settings"].push_back(varSetting);
            }
          }
        }
        found = true;

        if (doFilter)
          break;
      }
    }

    if (doFilter && !found)
      return InvalidParams;
  }

  return OK;
}

JSONRPC_STATUS CSettingsOperations::GetSettingValue(const CVariant& parameterObject,
                                                    CVariant& result)
{
  std::string settingId = parameterObject["setting"].asString();

  SettingPtr setting = CServiceBroker::GetSettingsComponent()->GetSettings()->GetSetting(settingId);
  if (setting == nullptr)
    return Fail(result, Reason::NoSuchSetting, Target("setting", parameterObject["setting"]));

  CVariant value;
  switch (setting->GetType())
  {
    case SettingType::Boolean:
      value = std::static_pointer_cast<CSettingBool>(setting)->GetValue();
      break;

    case SettingType::Integer:
      value = std::static_pointer_cast<CSettingInt>(setting)->GetValue();
      break;

    case SettingType::Number:
      value = std::static_pointer_cast<CSettingNumber>(setting)->GetValue();
      break;

    case SettingType::String:
      value = std::static_pointer_cast<CSettingString>(setting)->GetValue();
      break;

    case SettingType::List:
    {
      SerializeSettingListValues(
          CServiceBroker::GetSettingsComponent()->GetSettings()->GetList(settingId), value);
      break;
    }

    case SettingType::Unknown:
    case SettingType::Action:
    default:
      return InvalidParams;
  }

  result["value"] = value;

  return OK;
}

namespace
{
// A setting whose options come from a filler passes any value through CheckValidity. The filler
// runs only while nothing is cached: it can snap the current value to its best match.
bool IsListedOption(const std::shared_ptr<CSettingInt>& setting, int value)
{
  if (setting->GetOptionsType() != SettingOptionsType::Dynamic)
    return true;

  const IntegerSettingOptions& cached = setting->GetDynamicOptions();
  const IntegerSettingOptions options = cached.empty() ? setting->UpdateDynamicOptions() : cached;
  return std::ranges::any_of(options, [value](const IntegerSettingOption& option)
                             { return option.value == value; });
}

bool IsListedOption(const std::shared_ptr<CSettingString>& setting, const std::string& value)
{
  if (setting->GetOptionsType() != SettingOptionsType::Dynamic)
    return true;

  const StringSettingOptions& cached = setting->GetDynamicOptions();
  const StringSettingOptions options = cached.empty() ? setting->UpdateDynamicOptions() : cached;
  return std::ranges::any_of(options, [&value](const StringSettingOption& option)
                             { return option.value == value; });
}
} // namespace

JSONRPC_STATUS CSettingsOperations::SetSettingValue(const CVariant& parameterObject,
                                                    CVariant& result)
{
  std::string settingId = parameterObject["setting"].asString();
  CVariant value = parameterObject["value"];

  SettingPtr setting = CServiceBroker::GetSettingsComponent()->GetSettings()->GetSetting(settingId);
  if (setting == nullptr)
    return Fail(result, Reason::NoSuchSetting, Target("setting", parameterObject["setting"]));
  if (!setting->IsEnabled())
    return Fail(result, Reason::SettingDisabled, Target("setting", parameterObject["setting"]));

  // engaged for the rest of the call: a display mode change is kept without the prompt
  std::optional<CDisplaySettings::CConfirmedChange> confirmed;
  if (parameterObject["confirmed"].asBoolean())
    confirmed.emplace();

  bool changed = false;
  switch (setting->GetType())
  {
    case SettingType::Boolean:
      if (!value.isBoolean())
        return InvalidParams;

      changed = std::static_pointer_cast<CSettingBool>(setting)->SetValue(value.asBoolean());
      break;

    case SettingType::Integer:
    {
      if (!value.isInteger() && !value.isUnsignedInteger())
        return InvalidParams;

      const auto intSetting = std::static_pointer_cast<CSettingInt>(setting);
      const int intValue = static_cast<int>(value.asInteger());
      if (!intSetting->CheckValidity(intValue) || !IsListedOption(intSetting, intValue))
        return InvalidParams;

      changed = intSetting->SetValue(intValue);
      break;
    }

    case SettingType::Number:
    {
      if (!value.isDouble())
        return InvalidParams;

      const auto numberSetting = std::static_pointer_cast<CSettingNumber>(setting);
      if (!numberSetting->CheckValidity(value.asDouble()))
        return InvalidParams;

      changed = numberSetting->SetValue(value.asDouble());
      break;
    }

    case SettingType::String:
    {
      if (!value.isString())
        return InvalidParams;

      const auto stringSetting = std::static_pointer_cast<CSettingString>(setting);
      if (!stringSetting->CheckValidity(value.asString()) ||
          !IsListedOption(stringSetting, value.asString()))
        return InvalidParams;

      changed = stringSetting->SetValue(value.asString());
      break;
    }

    case SettingType::List:
    {
      if (!value.isArray())
        return InvalidParams;

      std::vector<CVariant> values;
      for (CVariant::const_iterator_array itValue = value.begin_array();
           itValue != value.end_array(); ++itValue)
        values.push_back(*itValue);

      changed = CServiceBroker::GetSettingsComponent()->GetSettings()->SetList(settingId, values);
      break;
    }

    case SettingType::Unknown:
    case SettingType::Action:
    default:
      return InvalidParams;
  }

  // A change handler declined the value, e.g. a display mode that was not kept.
  if (!changed)
    return Fail(result, Reason::ChangeDeclined, Target("setting", parameterObject["setting"]));

  result = true;
  return OK;
}

JSONRPC_STATUS CSettingsOperations::ResetSettingValue(const CVariant& parameterObject,
                                                      CVariant& result)
{
  std::string settingId = parameterObject["setting"].asString();

  SettingPtr setting = CServiceBroker::GetSettingsComponent()->GetSettings()->GetSetting(settingId);
  if (setting == nullptr)
    return Fail(result, Reason::NoSuchSetting, Target("setting", parameterObject["setting"]));
  if (!setting->IsEnabled())
    return Fail(result, Reason::SettingDisabled, Target("setting", parameterObject["setting"]));

  switch (setting->GetType())
  {
    case SettingType::Boolean:
    case SettingType::Integer:
    case SettingType::Number:
    case SettingType::String:
    case SettingType::List:
      setting->Reset();
      break;

    case SettingType::Unknown:
    case SettingType::Action:
    default:
      return InvalidParams;
  }

  return ACK;
}

SettingLevel CSettingsOperations::ParseSettingLevel(const std::string& strLevel)
{
  if (StringUtils::EqualsNoCase(strLevel, "basic"))
    return SettingLevel::Basic;
  if (StringUtils::EqualsNoCase(strLevel, "advanced"))
    return SettingLevel::Advanced;
  if (StringUtils::EqualsNoCase(strLevel, "expert"))
    return SettingLevel::Expert;

  return SettingLevel::Standard;
}

namespace
{
std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}

void SerializeLabels(const ISetting& setting, CVariant& obj)
{
  obj["label"] = Localize(setting.GetLabel());
  if (setting.GetHelp() >= 0)
    obj["help"] = Localize(setting.GetHelp());
}

template<typename Options>
CVariant SerializeOptions(const Options& options)
{
  CVariant result(CVariant::VariantTypeArray);
  for (const auto& option : options)
  {
    CVariant varOption(CVariant::VariantTypeObject);
    varOption["label"] = option.label;
    varOption["value"] = option.value;
    result.push_back(varOption);
  }
  return result;
}

CVariant SerializeOptions(const TranslatableIntegerSettingOptions& options)
{
  CVariant result(CVariant::VariantTypeArray);
  for (const auto& option : options)
  {
    CVariant varOption(CVariant::VariantTypeObject);
    varOption["label"] = Localize(option.label);
    varOption["value"] = option.value;
    result.push_back(varOption);
  }
  return result;
}

CVariant SerializeOptions(const TranslatableStringSettingOptions& options)
{
  CVariant result(CVariant::VariantTypeArray);
  for (const auto& [label, value] : options)
  {
    CVariant varOption(CVariant::VariantTypeObject);
    varOption["label"] = Localize(label);
    varOption["value"] = value;
    result.push_back(varOption);
  }
  return result;
}

//! The options of an integer or string setting, or nothing when it takes a free value
template<typename Setting>
std::optional<CVariant> SerializeOptionsOf(const std::shared_ptr<const Setting>& setting)
{
  switch (setting->GetOptionsType())
  {
    case SettingOptionsType::StaticTranslatable:
      return SerializeOptions(setting->GetTranslatableOptions());
    case SettingOptionsType::Static:
      return SerializeOptions(setting->GetOptions());
    case SettingOptionsType::Dynamic:
      return SerializeOptions(std::const_pointer_cast<Setting>(setting)->UpdateDynamicOptions());
    case SettingOptionsType::Unknown:
    default:
      return std::nullopt;
  }
}
} // unnamed namespace

bool CSettingsOperations::SerializeISetting(const std::shared_ptr<const ISetting>& setting,
                                            CVariant& obj)
{
  if (setting == nullptr)
    return false;

  obj["id"] = setting->GetId();

  return true;
}

bool CSettingsOperations::SerializeSettingSection(
    const std::shared_ptr<const CSettingSection>& setting, CVariant& obj)
{
  if (!SerializeISetting(setting, obj))
    return false;

  SerializeLabels(*setting, obj);
  return true;
}

bool CSettingsOperations::SerializeSettingCategory(
    const std::shared_ptr<const CSettingCategory>& setting, CVariant& obj)
{
  if (!SerializeISetting(setting, obj))
    return false;

  SerializeLabels(*setting, obj);
  return true;
}

bool CSettingsOperations::SerializeSettingGroup(const std::shared_ptr<const CSettingGroup>& setting,
                                                CVariant& obj)
{
  return SerializeISetting(setting, obj);
}

bool CSettingsOperations::SerializeSetting(const std::shared_ptr<const CSetting>& setting,
                                           CVariant& obj)
{
  if (!SerializeISetting(setting, obj))
    return false;

  SerializeLabels(*setting, obj);

  const char* const level = SettingLevelToString(setting->GetLevel());
  if (level == nullptr)
    return false;

  obj["level"] = level;
  obj["enabled"] = setting->IsEnabled();
  obj["parent"] = setting->GetParent();

  obj["control"] = CVariant(CVariant::VariantTypeObject);
  if (!SerializeSettingControl(setting->GetControl(), obj["control"]))
    return false;

  switch (setting->GetType())
  {
    case SettingType::Boolean:
      obj["type"] = "boolean";
      SerializeSettingBool(std::static_pointer_cast<const CSettingBool>(setting), obj);
      return true;

    case SettingType::Integer:
      obj["type"] = "integer";
      SerializeSettingInt(std::static_pointer_cast<const CSettingInt>(setting), obj);
      return true;

    case SettingType::Number:
      obj["type"] = "number";
      SerializeSettingNumber(std::static_pointer_cast<const CSettingNumber>(setting), obj);
      return true;

    case SettingType::String:
      obj["type"] = "string";
      SerializeSettingString(std::static_pointer_cast<const CSettingString>(setting), obj);
      return true;

    case SettingType::Action:
      obj["type"] = "action";
      obj["data"] = std::static_pointer_cast<const CSettingAction>(setting)->GetData();
      return true;

    case SettingType::List:
      obj["type"] = "list";
      return SerializeSettingList(std::static_pointer_cast<const CSettingList>(setting), obj);

    default:
      return false;
  }
}

void CSettingsOperations::SerializeSettingBool(const std::shared_ptr<const CSettingBool>& setting,
                                               CVariant& obj)
{
  obj["value"] = setting->GetValue();
  obj["default"] = setting->GetDefault();
}

void CSettingsOperations::SerializeSettingInt(const std::shared_ptr<const CSettingInt>& setting,
                                              CVariant& obj)
{
  obj["default"] = setting->GetDefault();

  if (std::optional<CVariant> options = SerializeOptionsOf(setting))
    obj["options"] = std::move(*options);
  else
  {
    obj["minimum"] = setting->GetMinimum();
    obj["step"] = setting->GetStep();
    obj["maximum"] = setting->GetMaximum();
  }

  // read after the options, whose update can change the value
  obj["value"] = setting->GetValue();
}

void CSettingsOperations::SerializeSettingNumber(
    const std::shared_ptr<const CSettingNumber>& setting, CVariant& obj)
{
  obj["value"] = setting->GetValue();
  obj["default"] = setting->GetDefault();

  obj["minimum"] = setting->GetMinimum();
  obj["step"] = setting->GetStep();
  obj["maximum"] = setting->GetMaximum();
}

void CSettingsOperations::SerializeSettingString(
    const std::shared_ptr<const CSettingString>& setting, CVariant& obj)
{
  obj["default"] = setting->GetDefault();

  obj["allowEmpty"] = setting->AllowEmpty();
  obj["allownewoption"] = setting->AllowNewOption();

  if (std::optional<CVariant> options = SerializeOptionsOf(setting))
    obj["options"] = std::move(*options);

  // read after the options, whose update can change the value
  obj["value"] = setting->GetValue();

  const std::string& format = setting->GetControl()->GetFormat();
  if (format == "path")
  {
    const auto path = std::static_pointer_cast<const CSettingPath>(setting);
    obj["type"] = "path";
    obj["writable"] = path->Writable();
    obj["sources"] = path->GetSources();
  }
  else if (format == "addon")
  {
    obj["type"] = "addon";
    obj["addonType"] = ADDON::CAddonInfo::TranslateType(
        std::static_pointer_cast<const CSettingAddon>(setting)->GetAddonType());
  }
  else if (format == "date" || format == "time")
    obj["type"] = format;
}

bool CSettingsOperations::SerializeSettingList(const std::shared_ptr<const CSettingList>& setting,
                                               CVariant& obj)
{
  if (!SerializeSetting(setting->GetDefinition(), obj["definition"]))
    return false;

  SerializeSettingListValues(CSettingUtils::GetList(setting), obj["value"]);
  SerializeSettingListValues(CSettingUtils::ListToValues(setting, setting->GetDefault()),
                             obj["default"]);

  obj["elementType"] = obj["definition"]["type"];
  obj["delimiter"] = setting->GetDelimiter();
  obj["minimumItems"] = setting->GetMinimumItems();
  obj["maximumItems"] = setting->GetMaximumItems();

  return true;
}

bool CSettingsOperations::SerializeSettingControl(
    const std::shared_ptr<const ISettingControl>& control, CVariant& obj)
{
  if (control == nullptr)
    return false;

  const std::string& type = control->GetType();
  obj["type"] = type;
  obj["format"] = control->GetFormat();
  obj["delayed"] = control->GetDelayed();

  if (type == "spinner")
  {
    const auto spinner = std::static_pointer_cast<const CSettingControlSpinner>(control);
    if (spinner->GetFormatLabel() >= 0)
      obj["formatLabel"] = Localize(spinner->GetFormatLabel());
    else if (!spinner->GetFormatString().empty() && spinner->GetFormatString() != "{:d}")
      obj["formatLabel"] = spinner->GetFormatString();
    if (spinner->GetMinimumLabel() >= 0)
      obj["minimumLabel"] = Localize(spinner->GetMinimumLabel());
  }
  else if (type == "edit")
  {
    const auto edit = std::static_pointer_cast<const CSettingControlEdit>(control);
    obj["hidden"] = edit->IsHidden();
    obj["verifyNewValue"] = edit->VerifyNewValue();
    if (edit->GetHeading() >= 0)
      obj["heading"] = Localize(edit->GetHeading());
  }
  else if (type == "button")
  {
    const auto button = std::static_pointer_cast<const CSettingControlButton>(control);
    if (button->GetHeading() >= 0)
      obj["heading"] = Localize(button->GetHeading());
  }
  else if (type == "list")
  {
    const auto list = std::static_pointer_cast<const CSettingControlList>(control);
    if (list->GetHeading() >= 0)
      obj["heading"] = Localize(list->GetHeading());
    obj["multiSelect"] = list->CanMultiSelect();
  }
  else if (type == "slider")
  {
    const auto slider = std::static_pointer_cast<const CSettingControlSlider>(control);
    if (slider->GetHeading() >= 0)
      obj["heading"] = Localize(slider->GetHeading());
    obj["popup"] = slider->UsePopup();
    if (slider->GetFormatLabel() >= 0)
      obj["formatLabel"] = Localize(slider->GetFormatLabel());
    else
      obj["formatLabel"] = slider->GetFormatString();
  }
  else if (type == "range")
  {
    const auto range = std::static_pointer_cast<const CSettingControlRange>(control);
    if (range->GetFormatLabel() >= 0)
      obj["formatLabel"] = Localize(range->GetFormatLabel());
    else
      obj["formatLabel"] = "";
    if (range->GetValueFormatLabel() >= 0)
      obj["formatValue"] = Localize(range->GetValueFormatLabel());
    else
      obj["formatValue"] = range->GetValueFormat();
  }
  else if (type != "toggle" && type != "label")
    return false;

  return true;
}

void CSettingsOperations::SerializeSettingListValues(const std::vector<CVariant>& values,
                                                     CVariant& obj)
{
  obj = CVariant(CVariant::VariantTypeArray);
  for (const auto& itValue : values)
    obj.push_back(itValue);
}

JSONRPC_STATUS CSettingsOperations::GetSkinSettings(const CVariant& parameterObject,
                                                    CVariant& result)
{
  const std::set<ADDON::CSkinSettingPtr> settings = CSkinSettings::GetInstance().GetSettings();
  CVariant varSettings(CVariant::VariantTypeArray);

  for (const auto& setting : settings)
  {
    CVariant varSetting(CVariant::VariantTypeObject);
    varSetting["id"] = setting->name;

    if (setting->GetType() == "bool")
    {
      varSetting["value"] = std::static_pointer_cast<ADDON::CSkinSettingBool>(setting)->value;
      varSetting["type"] = "boolean";
    }
    else if (setting->GetType() == "string")
    {
      varSetting["value"] = std::static_pointer_cast<ADDON::CSkinSettingString>(setting)->value;
      varSetting["type"] = setting->GetType();
    }
    else
      continue;

    varSettings.push_back(varSetting);
  }

  result["skin"] = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
      CSettings::SETTING_LOOKANDFEEL_SKIN);
  result["settings"] = varSettings;
  return OK;
}

JSONRPC_STATUS CSettingsOperations::GetSkinSettingValue(const CVariant& parameterObject,
                                                        CVariant& result)
{
  const std::string settingId = parameterObject["setting"].asString();
  ADDON::CSkinSettingPtr setting = CSkinSettings::GetInstance().GetSetting(settingId);

  if (setting == nullptr)
    return InvalidParams;

  CVariant value;
  if (setting->GetType() == "string")
    value = std::static_pointer_cast<ADDON::CSkinSettingString>(setting)->value;
  else if (setting->GetType() == "bool")
    value = std::static_pointer_cast<ADDON::CSkinSettingBool>(setting)->value;
  else
    return InvalidParams;

  result["value"] = value;
  return OK;
}

JSONRPC_STATUS CSettingsOperations::SetSkinSettingValue(const CVariant& parameterObject,
                                                        CVariant& result)
{
  const std::string settingId = parameterObject["setting"].asString();
  ADDON::CSkinSettingPtr setting = CSkinSettings::GetInstance().GetSetting(settingId);

  if (setting == nullptr)
    return InvalidParams;

  CVariant value = parameterObject["value"];
  if (setting->GetType() == "string")
  {
    if (!value.isString())
      return InvalidParams;

    result = std::static_pointer_cast<ADDON::CSkinSettingString>(setting)->value = value.asString();
  }
  else if (setting->GetType() == "bool")
  {
    if (!value.isBoolean())
      return InvalidParams;

    result = std::static_pointer_cast<ADDON::CSkinSettingBool>(setting)->value = value.asBoolean();
  }
  else
  {
    return InvalidParams;
  }

  return OK;
}
