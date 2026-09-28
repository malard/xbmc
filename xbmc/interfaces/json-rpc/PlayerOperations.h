/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemHandler.h"
#include "JSONRPC.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

class CVariant;

namespace PVR
{
class CPVRChannelGroup;
class CPVREpgInfoTag;
}

namespace KODI::PLAYLIST
{
enum class Type;
} // namespace KODI::PLAYLIST

namespace JSONRPC
{
enum PlayerType
{
  None,
  Video,
  Audio,
  Picture
};

class CPlayerOperations : CFileItemHandler
{
public:
  static JSONRPC_STATUS GetPlayers(const std::string& method,
                                   ITransportLayer* transport,
                                   IClient* client,
                                   const CVariant& parameterObject,
                                   CVariant& result);
  static JSONRPC_STATUS GetProperties(const std::string& method,
                                      ITransportLayer* transport,
                                      IClient* client,
                                      const CVariant& parameterObject,
                                      CVariant& result);
  static JSONRPC_STATUS GetItem(const std::string& method,
                                ITransportLayer* transport,
                                IClient* client,
                                const CVariant& parameterObject,
                                CVariant& result);

  static JSONRPC_STATUS PlayPause(const std::string& method,
                                  ITransportLayer* transport,
                                  IClient* client,
                                  const CVariant& parameterObject,
                                  CVariant& result);
  static JSONRPC_STATUS Stop(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS GetAudioDelay(const std::string& method,
                                      ITransportLayer* transport,
                                      IClient* client,
                                      const CVariant& parameterObject,
                                      CVariant& result);
  static JSONRPC_STATUS SetAudioDelay(const std::string& method,
                                      ITransportLayer* transport,
                                      IClient* client,
                                      const CVariant& parameterObject,
                                      CVariant& result);
  static JSONRPC_STATUS SetSpeed(const std::string& method,
                                 ITransportLayer* transport,
                                 IClient* client,
                                 const CVariant& parameterObject,
                                 CVariant& result);
  static JSONRPC_STATUS SetTempo(const std::string& method,
                                 ITransportLayer* transport,
                                 IClient* client,
                                 const CVariant& parameterObject,
                                 CVariant& result);
  static JSONRPC_STATUS Seek(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);

  static JSONRPC_STATUS Move(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS Zoom(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS SetViewMode(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);
  static JSONRPC_STATUS GetViewMode(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);
  static JSONRPC_STATUS Rotate(const std::string& method,
                               ITransportLayer* transport,
                               IClient* client,
                               const CVariant& parameterObject,
                               CVariant& result);

  static JSONRPC_STATUS Open(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS GoTo(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS SetShuffle(const std::string& method,
                                   ITransportLayer* transport,
                                   IClient* client,
                                   const CVariant& parameterObject,
                                   CVariant& result);
  static JSONRPC_STATUS SetRepeat(const std::string& method,
                                  ITransportLayer* transport,
                                  IClient* client,
                                  const CVariant& parameterObject,
                                  CVariant& result);
  static JSONRPC_STATUS SetPartymode(const std::string& method,
                                     ITransportLayer* transport,
                                     IClient* client,
                                     const CVariant& parameterObject,
                                     CVariant& result);

  static JSONRPC_STATUS SetAudioStream(const std::string& method,
                                       ITransportLayer* transport,
                                       IClient* client,
                                       const CVariant& parameterObject,
                                       CVariant& result);
  static JSONRPC_STATUS AddSubtitle(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);
  static JSONRPC_STATUS SetSubtitle(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);
  static JSONRPC_STATUS SetVideoStream(const std::string& method,
                                       ITransportLayer* transport,
                                       IClient* client,
                                       const CVariant& parameterObject,
                                       CVariant& result);

  static JSONRPC_STATUS GetChapters(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);

private:
  /*!
   * \brief The players a verb acts on: the one \p playlist names, or with none everything playing
   * (the playback and a slideshow beside it), or the playlist the player would act on.
   */
  static std::vector<PlayerType> GetTargets(const CVariant& playlist);
  //! The player a query answers for: the first of GetTargets().
  static PlayerType GetTarget(const CVariant& playlist);
  /*!
   * \brief Run a verb on the player for each target; it succeeds if it succeeds for any of them.
   * A named playlist that is not playing fails.
   */
  static JSONRPC_STATUS ForEachTarget(const CVariant& parameterObject,
                                      const std::function<JSONRPC_STATUS(PlayerType)>& verb);
  //! As ForEachTarget, for a verb about the playlist, which may name one that is not playing.
  static JSONRPC_STATUS ForEachOnList(const CVariant& parameterObject,
                                      const std::function<JSONRPC_STATUS(PlayerType)>& verb);
  /*!
   * \return The playlist a verb or property is about: the one named, else the playing one, else
   * the one matching the player; none for the slideshow.
   */
  static std::optional<KODI::PLAYLIST::Type> GetPlayList(PlayerType player, const CVariant& named);
  static JSONRPC_STATUS StartSlideshow(const std::string& path,
                                       bool recursive,
                                       bool random,
                                       const std::string& firstPicturePath = "");
  static void SendSlideshowAction(int actionID);
  static JSONRPC_STATUS GetPropertyValue(PlayerType player,
                                         const std::string& property,
                                         CVariant& result,
                                         const CVariant& named = CVariant());

  static bool IsPVRChannel();
  static std::shared_ptr<PVR::CPVREpgInfoTag> GetCurrentEpg();
};
}
