/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUISourcesAnnouncementHandler.h"

#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/AnnouncementMessages.h"

CGUISourcesAnnouncementHandler::CGUISourcesAnnouncementHandler()
{
  CServiceBroker::GetAnnouncementManager()->AddAnnouncer(this, ANNOUNCEMENT::Sources);
}

CGUISourcesAnnouncementHandler::~CGUISourcesAnnouncementHandler()
{
  CServiceBroker::GetAnnouncementManager()->RemoveAnnouncer(this);
}

void CGUISourcesAnnouncementHandler::Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                                              const std::string& sender,
                                              const std::string& message,
                                              const CVariant& data)
{
  if (message == ANNOUNCEMENT::MESSAGE::ON_ADDED || message == ANNOUNCEMENT::MESSAGE::ON_REMOVED ||
      message == ANNOUNCEMENT::MESSAGE::ON_UPDATED)
  {
    CGUIMessage message(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_PATH);
    message.SetStringParam(data.asString());
    CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(message);
  }
}
