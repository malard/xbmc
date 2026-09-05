/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIOperations.h"

#include "GUIInfoManager.h"
#include "MessengerPayload.h"
#include "ServiceBroker.h"
#include "addons/AddonManager.h"
#include "addons/IAddon.h"
#include "addons/addoninfo/AddonType.h"
#include "application/Application.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/StereoscopicsManager.h"
#include "guilib/WindowIDs.h"
#include "input/WindowTranslator.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "messaging/ApplicationMessenger.h"
#include "powermanagement/PowerManager.h"
#include "rendering/RenderSystem.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/windows/GUIWindowScreenAlignment.h"
#include "utils/AspectRatioVocabulary.h"
#include "utils/Screenshot.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace JSONRPC;
using namespace ADDON;

namespace
{
CGUIWindowScreenAlignment* GetScreenAlignmentWindow()
{
  auto* const gui = CServiceBroker::GetGUI();
  if (!gui)
    return nullptr;

  return gui->GetWindowManager().GetWindow<CGUIWindowScreenAlignment>(WINDOW_SCREEN_ALIGNMENT);
}

//! \brief A ratio as the API states them, rounded to four places so a caller can compare it.
double PublishedAspect(float aspect)
{
  return std::round(static_cast<double>(aspect) * 10000.0) / 10000.0;
}
} // unnamed namespace

JSONRPC_STATUS CGUIOperations::GetProperties(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CVariant properties = CVariant(CVariant::VariantTypeObject);
  for (unsigned int index = 0; index < parameterObject["properties"].size(); index++)
  {
    std::string propertyName = parameterObject["properties"][index].asString();
    CVariant property;
    JSONRPC_STATUS ret;
    if ((ret = GetPropertyValue(propertyName, property)) != OK)
      return ret;

    properties[propertyName] = property;
  }

  result = properties;

  return OK;
}

JSONRPC_STATUS CGUIOperations::ActivateWindow(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int iWindow = CWindowTranslator::TranslateWindow(parameterObject["window"].asString());
  if (iWindow != WINDOW_INVALID)
  {
    std::vector<std::string> params;
    for (CVariant::const_iterator_array param = parameterObject["parameters"].begin_array();
         param != parameterObject["parameters"].end_array(); ++param)
    {
      if (param->isString() && !param->empty())
        params.push_back(param->asString());
    }
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_GUI_ACTIVATE_WINDOW, iWindow, 0, nullptr, "",
                                               params);
    return ACK;
  }

  return InvalidParams;
}

JSONRPC_STATUS CGUIOperations::ShowNotification(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  std::string image = parameterObject["image"].asString();
  std::string title = parameterObject["title"].asString();
  std::string message = parameterObject["message"].asString();
  unsigned int displaytime = (unsigned int)parameterObject["displaytime"].asUnsignedInteger();

  if (image.compare("info") == 0)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, title, message, displaytime);
  else if (image.compare("warning") == 0)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, title, message, displaytime);
  else if (image.compare("error") == 0)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, title, message, displaytime);
  else
    CGUIDialogKaiToast::QueueNotification(image, title, message, displaytime);

  return ACK;
}

JSONRPC_STATUS CGUIOperations::SetFullscreen(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  if ((parameterObject["fullscreen"].isString() &&
       parameterObject["fullscreen"].asString().compare("toggle") == 0) ||
      (parameterObject["fullscreen"].isBoolean() &&
       parameterObject["fullscreen"].asBoolean() != g_application.IsFullScreen()))
  {
    CServiceBroker::GetAppMessenger()->SendMsg(
        TMSG_GUI_ACTION, WINDOW_INVALID, -1,
        TransferToMessenger(std::make_unique<CAction>(ACTION_SHOW_GUI)));
  }
  else if (!parameterObject["fullscreen"].isBoolean() && !parameterObject["fullscreen"].isString())
    return InvalidParams;

  return GetPropertyValue("fullscreen", result);
}

JSONRPC_STATUS CGUIOperations::SetStereoscopicMode(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CAction action = CStereoscopicsManager::ConvertActionCommandToAction("SetStereoMode", parameterObject["mode"].asString());
  if (action.GetID() != ACTION_NONE)
  {
    CServiceBroker::GetAppMessenger()->SendMsg(
        TMSG_GUI_ACTION, WINDOW_INVALID, -1,
        TransferToMessenger(std::make_unique<CAction>(action)));
    return ACK;
  }

  return InvalidParams;
}

JSONRPC_STATUS CGUIOperations::GetStereoscopicModes(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  for (int i = static_cast<int>(RenderStereoMode::OFF);
       i < static_cast<int>(RenderStereoMode::COUNT); i++)
  {
    RenderStereoMode mode = static_cast<RenderStereoMode>(i);
    if (CServiceBroker::GetRenderSystem()->SupportsStereo(mode))
      result["stereoscopicmodes"].push_back(GetStereoModeObjectFromGuiMode(mode));
  }

  return OK;
}

JSONRPC_STATUS CGUIOperations::ActivateScreenSaver(const std::string& method,
                                                   ITransportLayer* transport,
                                                   IClient* client,
                                                   const CVariant& parameterObject,
                                                   CVariant& result)
{
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_ACTIVATESCREENSAVER);
  return ACK;
}

JSONRPC_STATUS CGUIOperations::TakeScreenshot(const std::string& method,
                                              ITransportLayer* transport,
                                              IClient* client,
                                              const CVariant& parameterObject,
                                              CVariant& result)
{
  using KODI::RENDERING::CAPTURE::CaptureContent;

  const std::string content = parameterObject["content"].asString();

  if (content == "video" || content == "both")
  {
    const auto& components = CServiceBroker::GetAppComponents();
    const auto appPlayer = components.GetComponent<CApplicationPlayer>();
    if (!appPlayer->IsRenderingVideo())
      return FailedToExecute;
  }

  const CaptureContent capture = content == "video"  ? CaptureContent::VIDEO
                                 : content == "both" ? CaptureContent::BOTH
                                                     : CaptureContent::COMPOSITE;

  const CScreenShot::ScreenshotFiles files =
      CScreenShot::TakeScreenshotSync(capture, parameterObject["target"].asString());

  switch (files.error)
  {
    case CScreenShot::ScreenshotError::NO_FOLDER:
      return Unavailable;
    case CScreenShot::ScreenshotError::BAD_TARGET:
      return InvalidParams;
    case CScreenShot::ScreenshotError::NOT_FOUND:
    case CScreenShot::ScreenshotError::FAILED:
      return FailedToExecute;
    case CScreenShot::ScreenshotError::NONE:
      break;
  }

  if (!files.composite.empty())
    result["composite"] = files.composite;
  if (!files.video.empty())
    result["video"] = files.video;

  return OK;
}

JSONRPC_STATUS CGUIOperations::DeleteScreenshots(const std::string& method,
                                                 ITransportLayer* transport,
                                                 IClient* client,
                                                 const CVariant& parameterObject,
                                                 CVariant& result)
{
  if (!CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_jsonAllowScreenshotDeletion)
    return Unavailable;

  const CScreenShot::ScreenshotDeletion removed =
      CScreenShot::DeleteScreenshots(parameterObject["file"].asString());

  switch (removed.error)
  {
    case CScreenShot::ScreenshotError::BAD_TARGET:
      return InvalidParams;
    case CScreenShot::ScreenshotError::NOT_FOUND:
      return NotFound;
    case CScreenShot::ScreenshotError::NO_FOLDER:
      return Unavailable;
    case CScreenShot::ScreenshotError::FAILED:
      return FailedToExecute;
    case CScreenShot::ScreenshotError::NONE:
      break;
  }

  result["deleted"] = removed.deleted;
  return OK;
}

JSONRPC_STATUS CGUIOperations::SetScreenAlignment(const std::string& method,
                                                  ITransportLayer* transport,
                                                  IClient* client,
                                                  const CVariant& parameterObject,
                                                  CVariant& result)
{
  CGUIWindowScreenAlignment* const window = GetScreenAlignmentWindow();
  if (!window)
    return FailedToExecute;

  // Null is the only value meaning "unstated": the service description materialises an absent
  // parameter as its type's empty value, so an omitted array arrives indistinguishable from the
  // empty one that clears every frame.
  if (!parameterObject["ratios"].isNull())
  {
    std::vector<float> ratios;
    for (CVariant::const_iterator_array ratio = parameterObject["ratios"].begin_array();
         ratio != parameterObject["ratios"].end_array(); ++ratio)
      ratios.push_back(ratio->asFloat());

    window->SetShownRatios(ratios);
  }

  if (!parameterObject["show"].isNull())
  {
    CGUIWindowManager& windowManager = CServiceBroker::GetGUI()->GetWindowManager();
    const bool show = parameterObject["show"].asBoolean();

    if (show != windowManager.IsWindowActive(WINDOW_SCREEN_ALIGNMENT))
    {
      if (show)
        CServiceBroker::GetAppMessenger()->SendMsg(TMSG_GUI_ACTIVATE_WINDOW,
                                                   WINDOW_SCREEN_ALIGNMENT, 0);
      else
        CServiceBroker::GetAppMessenger()->SendMsg(TMSG_GUI_PREVIOUS_WINDOW);
    }
  }

  result = GetScreenAlignmentState();
  return OK;
}

JSONRPC_STATUS CGUIOperations::GetScreenAlignment(const std::string& method,
                                                  ITransportLayer* transport,
                                                  IClient* client,
                                                  const CVariant& parameterObject,
                                                  CVariant& result)
{
  if (!GetScreenAlignmentWindow())
    return FailedToExecute;

  result = GetScreenAlignmentState();
  return OK;
}

CVariant CGUIOperations::GetScreenAlignmentState()
{
  CVariant state(CVariant::VariantTypeObject);

  state["showing"] =
      CServiceBroker::GetGUI()->GetWindowManager().IsWindowActive(WINDOW_SCREEN_ALIGNMENT);

  state["ratios"] = CVariant(CVariant::VariantTypeArray);
  if (const CGUIWindowScreenAlignment* const window = GetScreenAlignmentWindow())
  {
    for (const float ratio : window->ShownRatios())
      state["ratios"].push_back(PublishedAspect(ratio));
  }

  state["available"] = CVariant(CVariant::VariantTypeArray);
  for (const KODI::UTILS::AspectRatioEntry& entry : KODI::UTILS::CAspectRatioVocabulary::Entries())
    state["available"].push_back(PublishedAspect(entry.ratio));

  return state;
}

JSONRPC_STATUS CGUIOperations::GetPropertyValue(const std::string &property, CVariant &result)
{
  if (property == "currentwindow")
  {
    result["label"] = CServiceBroker::GetGUI()->GetInfoManager().GetLabel(
        CServiceBroker::GetGUI()->GetInfoManager().TranslateString("System.CurrentWindow"),
        INFO::DEFAULT_CONTEXT);
    result["id"] = CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindowOrDialog();
  }
  else if (property == "currentcontrol")
    result["label"] = CServiceBroker::GetGUI()->GetInfoManager().GetLabel(
        CServiceBroker::GetGUI()->GetInfoManager().TranslateString("System.CurrentControl"),
        INFO::DEFAULT_CONTEXT);
  else if (property == "skin")
  {
    std::string skinId = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_LOOKANDFEEL_SKIN);
    AddonPtr addon;
    if (!CServiceBroker::GetAddonMgr().GetAddon(skinId, addon, AddonType::SKIN,
                                                OnlyEnabled::CHOICE_YES))
      return InternalError;

    result["id"] = skinId;
    if (addon.get())
      result["name"] = addon->Name();
  }
  else if (property == "fullscreen")
    result = g_application.IsFullScreen();
  else if (property == "ready")
    result =
        g_application.IsInitialized() && CServiceBroker::GetGUI()->GetWindowManager().Initialized();
  else if (property == "stereoscopicmode")
  {
    const CStereoscopicsManager &stereoscopicsManager = CServiceBroker::GetGUI()->GetStereoscopicsManager();

    result = GetStereoModeObjectFromGuiMode(stereoscopicsManager.GetStereoMode());
  }
  else
    return InvalidParams;

  return OK;
}

JSONRPC_STATUS CGUIOperations::GetInfoLabels(const std::string& method,
                                             ITransportLayer* transport,
                                             IClient* client,
                                             const CVariant& parameterObject,
                                             CVariant& result)
{
  std::vector<std::string> info;

  for (unsigned int i = 0; i < parameterObject["labels"].size(); i++)
  {
    std::string field = parameterObject["labels"][i].asString();
    StringUtils::ToLower(field);

    info.push_back(parameterObject["labels"][i].asString());
  }

  if (!info.empty())
  {
    std::vector<std::string> infoLabels;
    CServiceBroker::GetAppMessenger()->SendMsg(TMSG_GUI_INFOLABEL, -1, -1,
                                               LendToMessenger(infoLabels), "", info);

    for (unsigned int i = 0; i < info.size(); i++)
    {
      if (i >= infoLabels.size())
        break;
      result[info[i]] = infoLabels[i];
    }
  }

  return OK;
}

JSONRPC_STATUS CGUIOperations::GetInfoBooleans(const std::string& method,
                                               ITransportLayer* transport,
                                               IClient* client,
                                               const CVariant& parameterObject,
                                               CVariant& result)
{
  std::vector<std::string> info;

  bool CanControlPower = (client->GetPermissionFlags() & ControlPower) > 0;

  for (unsigned int i = 0; i < parameterObject["booleans"].size(); i++)
  {
    std::string field = parameterObject["booleans"][i].asString();
    StringUtils::ToLower(field);

    // Need to override power management of whats in infomanager since jsonrpc
    // have a security layer aswell.
    if (field == "system.canshutdown" || field == "system.canpowerdown")
      result[parameterObject["booleans"][i].asString()] =
          (CServiceBroker::GetPowerManager().CanPowerdown() && CanControlPower);
    else if (field == "system.cansuspend")
      result[parameterObject["booleans"][i].asString()] =
          (CServiceBroker::GetPowerManager().CanSuspend() && CanControlPower);
    else if (field == "system.canhibernate")
      result[parameterObject["booleans"][i].asString()] =
          (CServiceBroker::GetPowerManager().CanHibernate() && CanControlPower);
    else if (field == "system.canreboot")
      result[parameterObject["booleans"][i].asString()] =
          (CServiceBroker::GetPowerManager().CanReboot() && CanControlPower);
    else
      info.push_back(parameterObject["booleans"][i].asString());
  }

  if (!info.empty())
  {
    std::vector<bool> infoLabels;
    CServiceBroker::GetAppMessenger()->SendMsg(TMSG_GUI_INFOBOOL, -1, -1,
                                               LendToMessenger(infoLabels), "", info);
    for (unsigned int i = 0; i < info.size(); i++)
    {
      if (i >= infoLabels.size())
        break;
      result[info[i].c_str()] = CVariant(infoLabels[i]);
    }
  }

  return OK;
}

CVariant CGUIOperations::GetStereoModeObjectFromGuiMode(const RenderStereoMode mode)
{
  const CStereoscopicsManager &stereoscopicsManager = CServiceBroker::GetGUI()->GetStereoscopicsManager();

  CVariant modeObj(CVariant::VariantTypeObject);
  modeObj["mode"] = stereoscopicsManager.ConvertGuiStereoModeToString(mode);
  modeObj["label"] = stereoscopicsManager.GetLabelForStereoMode(mode);
  return modeObj;
}
