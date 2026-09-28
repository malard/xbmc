/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaybackModes.h"

#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "messaging/ApplicationMessenger.h"
#include "pictures/SlideShowDelegator.h"
#include "utils/Variant.h"

#include <string>

using namespace KODI;

namespace JSONRPC
{

JSONRPC_STATUS ApplyShuffle(PLAYLIST::Type type, const CVariant& shuffle)
{
  const auto playLists = CServiceBroker::GetPlayLists();
  const bool before = playLists->IsShuffled(type);
  if (!shuffle.isBoolean() || shuffle.asBoolean() != before)
    CServiceBroker::GetAppMessenger()->SendMsg(
        TMSG_PLAYLISTPLAYER_SHUFFLE, static_cast<int>(type),
        shuffle.isBoolean() ? static_cast<int>(shuffle.asBoolean()) : -1);

  const bool after = playLists->IsShuffled(type);
  if (shuffle.isBoolean() ? after != shuffle.asBoolean() : after == before)
    return FailedToExecute;
  return ACK;
}

JSONRPC_STATUS ApplyRepeat(PLAYLIST::Type type, const CVariant& repeat)
{
  const auto playLists = CServiceBroker::GetPlayLists();
  const std::string wanted = repeat.asString();
  const CApplicationPlayLists::Repeat before = playLists->GetRepeat(type);

  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_PLAYLISTPLAYER_REPEAT, static_cast<int>(type), -1,
                                             nullptr, wanted);

  const CApplicationPlayLists::Repeat after = playLists->GetRepeat(type);
  if (wanted == "cycle")
    return after == before ? FailedToExecute : ACK;
  return CApplicationPlayLists::ParseRepeat(wanted) == after ? ACK : FailedToExecute;
}

JSONRPC_STATUS ShuffleSlideshow(const CVariant& shuffle)
{
  CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
  if (slideShow.NumSlides() < 0)
    return FailedToExecute;

  const bool toggle = shuffle.isString() && shuffle.asString() == "toggle";
  if (slideShow.IsShuffled())
    return (shuffle.isBoolean() && !shuffle.asBoolean()) || toggle ? FailedToExecute : ACK;

  if ((shuffle.isBoolean() && shuffle.asBoolean()) || toggle)
    slideShow.Shuffle();
  return ACK;
}

} // namespace JSONRPC
