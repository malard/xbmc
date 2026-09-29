/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItemHandler.h"

#include "AudioLibrary.h"
#include "DbUrl.h"
#include "FileItemList.h"
#include "FileOperations.h"
#include "JSONServiceDescription.h"
#include "PVREpgFields.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "VideoLibrary.h"
#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/pvr/pvr_epg.h" // EPG_TAG_INVALID_UID
#include "filesystem/Directory.h"
#include "imagefiles/ImageFileURL.h"
#include "interfaces/AnnouncementManager.h"
#include "music/MusicThumbLoader.h"
#include "music/tags/MusicInfoTag.h"
#include "pictures/PictureInfoTag.h"
#include "pvr/PVRManager.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/channels/PVRChannelGroupMember.h"
#include "pvr/epg/EpgInfoTag.h"
#include "pvr/recordings/PVRRecording.h"
#include "pvr/recordings/PVRRecordings.h"
#include "pvr/timers/PVRTimerInfoTag.h"
#include "pvr/timers/PVRTimers.h"
#include "utils/Artwork.h"
#include "utils/FileUtils.h"
#include "utils/ISerializable.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"
#include "video/VideoThumbLoader.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string.h>
#include <string>
#include <vector>

using namespace MUSIC_INFO;
using namespace JSONRPC;
using namespace XFILE;

namespace
{
/*!
 \brief The members of Playlist.Item that name a library entry by its identifier
 \return the identifier names, empty when the service description has not been parsed
 */
std::set<std::string> LibraryIdentifiers()
{
  std::set<std::string> identifiers;

  const JSONSchemaTypeDefinitionPtr item{CJSONServiceDescription::GetType("Playlist.Item")};
  const JSONSchemaTypeDefinitionPtr libraryId{CJSONServiceDescription::GetType("Library.Id")};
  if (!item || !libraryId)
    return identifiers;

  for (const auto& alternative : item->unionTypes)
  {
    for (auto property = alternative->properties.begin(); property != alternative->properties.end();
         ++property)
    {
      if (property->second->referencedType == libraryId)
        identifiers.insert(property->first);
    }
  }

  return identifiers;
}

//! A started thumbnail loader for items like \p item, or none for an item no loader serves
std::unique_ptr<CThumbLoader> ThumbLoaderFor(const CFileItem& item)
{
  std::unique_ptr<CThumbLoader> loader;
  if (item.HasVideoInfoTag())
    loader = std::make_unique<CVideoThumbLoader>();
  else if (item.HasMusicInfoTag())
    loader = std::make_unique<CMusicThumbLoader>();

  if (loader)
    loader->OnLoaderStart();
  return loader;
}

bool IsLibraryItem(const CFileItem& item)
{
  return (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_iDbId > -1) ||
         (item.HasMusicInfoTag() && item.GetMusicInfoTag()->GetDatabaseId() > -1);
}
} // unnamed namespace

bool CFileItemHandler::GetField(const std::string& field,
                                const CVariant& info,
                                const std::shared_ptr<CFileItem>& item,
                                CVariant& result,
                                bool& fetchedArt,
                                std::optional<std::shared_ptr<PVR::CPVRRecording>>& epgRecording,
                                std::optional<std::shared_ptr<PVR::CPVRTimerInfoTag>>& epgTimer,
                                CThumbLoader* thumbLoader /* = nullptr */)
{
  if (result.isMember(field) && !result[field].empty())
    return true;

  // overwrite serialized values
  if (item)
  {
    if (field == "mimeType" && item->GetMimeType().empty())
    {
      item->FillInMimeType(false);
      result[field] = item->GetMimeType();
      return true;
    }

    if (item->HasPVRChannelInfoTag())
    {
      // Translate PVR.Details.Broadcast -> List.Item.Base format
      if (field == "cast")
      {
        // string -> Video.Cast
        result[field] = TranslateEpgCast(info[field].asString());
        return true;
      }
      else if (field == "director" || field == "writer")
      {
        // string -> Array.String
        result[field] = StringUtils::Split(info[field].asString(), EPG_STRING_TOKEN_SEPARATOR);
        return true;
      }
      else if (field == "isRecording")
      {
        result[field] = CServiceBroker::GetPVRManager().Timers()->IsRecordingOnChannel(
            *item->GetPVRChannelInfoTag());
        return true;
      }
      else if (field == "broadcastNow" || field == "broadcastNext")
      {
        // Both slots are PVR.Details.Broadcast, whose label and field set only the handler supplies
        const std::shared_ptr<const PVR::CPVRChannel> channel{item->GetPVRChannelInfoTag()};
        const std::shared_ptr<PVR::CPVREpgInfoTag> tag{
            field == "broadcastNow" ? channel->GetEPGNow() : channel->GetEPGNext()};
        if (tag)
        {
          HandleFileItem("broadcastId", false, field.c_str(), std::make_shared<CFileItem>(tag),
                         CVariant{CVariant::VariantTypeObject}, BroadcastFields(), result, false);
        }
        return true;
      }
    }

    if (item->HasEPGInfoTag())
    {
      if (field == "hasTimer" || field == "hasReminder" || field == "hasTimerRule")
      {
        if (!epgTimer.has_value())
        {
          epgTimer =
              CServiceBroker::GetPVRManager().Timers()->GetTimerForEpgTag(item->GetEPGInfoTag());
        }

        const std::shared_ptr<PVR::CPVRTimerInfoTag>& timer{*epgTimer};
        if (field == "hasTimer")
          result[field] = (timer != nullptr);
        else if (field == "hasReminder")
          result[field] = (timer && timer->IsReminder());
        else
          result[field] = (timer && timer->HasParent());
        return true;
      }
      else if (field == "hasRecording" || field == "recording" || field == "recordingId")
      {
        if (!epgRecording.has_value())
        {
          epgRecording = CServiceBroker::GetPVRManager().Recordings()->GetRecordingForEpgTag(
              item->GetEPGInfoTag());
        }

        const std::shared_ptr<PVR::CPVRRecording>& recording{*epgRecording};
        if (field == "hasRecording")
        {
          result[field] = (recording != nullptr);
        }
        else if (field == "recording")
        {
          result[field] = recording ? recording->m_strFileNameAndPath : "";
        }
        else
        {
          result[field] = recording ? static_cast<int>(recording->RecordingID()) : -1;
        }
        return true;
      }
    }
  }

  // check for serialized values
  if (info.isMember(field) && !info[field].isNull())
  {
    result[field] = info[field];
    return true;
  }

  // check if the field requires special handling
  if (item)
  {
    // item properties keep Kodi's own lowercase names
    const std::string property = StringUtils::ToLower(std::string_view{field});

    if (item->IsAlbum())
    {
      if (field == "albumLabel")
      {
        result[field] = item->GetProperty("album_label");
        return true;
      }
      if (item->HasProperty("album_" + property + "_array"))
      {
        result[field] = item->GetProperty("album_" + property + "_array");
        return true;
      }
      if (item->HasProperty("album_" + property))
      {
        result[field] = item->GetProperty("album_" + property);
        return true;
      }
    }

    if (item->HasProperty("artist_" + property + "_array"))
    {
      result[field] = item->GetProperty("artist_" + property + "_array");
      return true;
    }
    if (item->HasProperty("artist_" + property))
    {
      result[field] = item->GetProperty("artist_" + property);
      return true;
    }

    const auto fillLibraryArt = [&](bool missing)
    {
      if (thumbLoader && missing && !fetchedArt && IsLibraryItem(*item))
      {
        thumbLoader->FillLibraryArt(*item);
        fetchedArt = true;
      }
    };

    if (field == "art")
    {
      fillLibraryArt(!item->GetProperty("libraryartfilled").asBoolean());

      const KODI::ART::Artwork& artMap = item->GetArt();
      CVariant artObj(CVariant::VariantTypeObject);
      for (const auto& artIt : artMap)
      {
        if (!artIt.second.empty())
          artObj[artIt.first] = IMAGE_FILES::URLFromFile(artIt.second);
      }

      result["art"] = artObj;
      return true;
    }

    if (field == "thumbnail")
    {
      if (thumbLoader && !item->HasArt("thumb") && !fetchedArt && IsLibraryItem(*item))
        fillLibraryArt(true);
      else if (item->HasPictureInfoTag() && !item->HasArt("thumb"))
        item->SetArt("thumb", IMAGE_FILES::URLFromFile(item->GetPath()));

      result["thumbnail"] =
          item->HasArt("thumb") ? IMAGE_FILES::URLFromFile(item->GetArt("thumb")) : "";
      return true;
    }

    if (field == "fanart")
    {
      fillLibraryArt(!item->HasArt("fanart"));
      result["fanart"] =
          item->HasArt("fanart") ? IMAGE_FILES::URLFromFile(item->GetArt("fanart")) : "";
      return true;
    }

    if (item->HasVideoInfoTag() && item->GetVideoContentType() == VideoDbContentType::TVSHOWS)
    {
      if (item->GetVideoInfoTag()->m_iSeason < 0 && field == "season")
      {
        result[field] = static_cast<int>(item->GetProperty("totalseasons").asInteger());
        return true;
      }
      if (field == "watchedEpisodes")
      {
        result[field] = static_cast<int>(item->GetProperty("watchedepisodes").asInteger());
        return true;
      }
    }

    if (item->HasProperty(property))
    {
      result[field] = item->GetProperty(property);
      return true;
    }
  }

  return false;
}

void CFileItemHandler::FillDetails(const ISerializable* info,
                                   const std::shared_ptr<CFileItem>& item,
                                   std::set<std::string>& fields,
                                   CVariant& result,
                                   CThumbLoader* thumbLoader /* = nullptr */)
{
  if (info == nullptr || fields.empty())
    return;

  CVariant serialization;
  info->Serialize(serialization);

  bool fetchedArt = false;
  std::optional<std::shared_ptr<PVR::CPVRRecording>> epgRecording;
  std::optional<std::shared_ptr<PVR::CPVRTimerInfoTag>> epgTimer;

  std::set<std::string> originalFields = fields;

  for (const auto& fieldIt : originalFields)
  {
    if (GetField(fieldIt, serialization, item, result, fetchedArt, epgRecording, epgTimer,
                 thumbLoader) &&
        result.isMember(fieldIt) && !result[fieldIt].empty())
      fields.erase(fieldIt);
  }
}

void CFileItemHandler::HandleFileItemList(const char* ID,
                                          bool allowFile,
                                          const char* resultname,
                                          CFileItemList& items,
                                          const CVariant& parameterObject,
                                          CVariant& result,
                                          bool sortLimit /* = true */)
{
  HandleFileItemList(ID, allowFile, resultname, items, parameterObject, result, items.Size(),
                     sortLimit);
}

void CFileItemHandler::HandleFileItemList(const char* ID,
                                          bool allowFile,
                                          const char* resultname,
                                          CFileItemList& items,
                                          const CVariant& parameterObject,
                                          CVariant& result,
                                          int size,
                                          bool sortLimit /* = true */)
{
  int start, end;
  HandleLimits(parameterObject, result, size, start, end);

  if (sortLimit)
    Sort(items, parameterObject);
  else
  {
    start = 0;
    end = items.Size();
  }

  const std::unique_ptr<CThumbLoader> thumbLoader =
      end - start > 0 ? ThumbLoaderFor(*items.Get(start)) : nullptr;

  const std::set<std::string> fields{RequestedFields(parameterObject)};

  result[resultname].reserve(static_cast<size_t>(end - start));
  for (int i = start; i < end; i++)
  {
    CFileItemPtr item = items.Get(i);
    HandleFileItem(ID, allowFile, resultname, item, parameterObject, fields, result, true,
                   thumbLoader.get());
  }
}

void CFileItemHandler::HandleFileItem(const char* ID,
                                      bool allowFile,
                                      const char* resultname,
                                      const std::shared_ptr<CFileItem>& item,
                                      const CVariant& parameterObject,
                                      const CVariant& validFields,
                                      CVariant& result,
                                      bool append /* = true */,
                                      CThumbLoader* thumbLoader /* = nullptr */)
{
  HandleFileItem(ID, allowFile, resultname, item, parameterObject, FieldNames(validFields), result,
                 append, thumbLoader);
}

void CFileItemHandler::HandleFileItem(const char* ID,
                                      bool allowFile,
                                      const char* resultname,
                                      const std::shared_ptr<CFileItem>& item,
                                      const CVariant& parameterObject,
                                      const std::set<std::string>& validFields,
                                      CVariant& result,
                                      bool append /* = true */,
                                      CThumbLoader* thumbLoader /* = nullptr */)
{
  CVariant object;
  std::set<std::string> fields(validFields.begin(), validFields.end());

  if (item.get())
  {
    if (fields.erase("file") > 0 && allowFile)
    {
      // A folder reports its own path so that file agrees with filetype
      if (fields.contains("fileType") && item->IsFolder())
        object["file"] = item->GetPath();
      else if (item->HasVideoInfoTag() && !item->GetVideoInfoTag()->GetPath().empty())
        object["file"] = item->GetVideoInfoTag()->GetPath();
      if (item->HasMusicInfoTag() && !item->GetMusicInfoTag()->GetURL().empty())
        object["file"] = item->GetMusicInfoTag()->GetURL();
      if (item->HasPVRTimerInfoTag() && !item->GetPVRTimerInfoTag()->Path().empty())
        object["file"] = item->GetPVRTimerInfoTag()->Path();

      if (!object.isMember("file"))
        object["file"] = item->GetDynPath();
    }

    if (item->HasProperty("playlistdisplayorder"))
    {
      object["position"] = item->GetProperty("playlistposition");
      object["displayOrder"] = item->GetProperty("playlistdisplayorder");
    }

    if (fields.erase("mediaPath") > 0)
      object["mediaPath"] = item->GetPath();
    if (fields.erase("dynPath") > 0)
      object["dynPath"] = item->GetDynPath();

    if (ID)
    {
      if (item->HasPVRChannelInfoTag() && item->GetPVRChannelInfoTag()->ChannelID() > 0)
        object[ID] = item->GetPVRChannelInfoTag()->ChannelID();
      else if (item->HasEPGInfoTag() && item->GetEPGInfoTag()->DatabaseID() > 0)
        object[ID] = item->GetEPGInfoTag()->DatabaseID();
      else if (item->HasPVRRecordingInfoTag() && item->GetPVRRecordingInfoTag()->RecordingID() > 0)
        object[ID] = item->GetPVRRecordingInfoTag()->RecordingID();
      else if (item->HasPVRTimerInfoTag() && item->GetPVRTimerInfoTag()->TimerID() > 0)
        object[ID] = item->GetPVRTimerInfoTag()->TimerID();
      else if (item->HasMusicInfoTag() && item->GetMusicInfoTag()->GetDatabaseId() > 0)
        object[ID] = item->GetMusicInfoTag()->GetDatabaseId();
      else if (item->HasVideoInfoTag() && item->GetVideoInfoTag()->m_iDbId > 0)
        object[ID] = item->GetVideoInfoTag()->m_iDbId;

      if (StringUtils::CompareNoCase(ID, "id") == 0)
      {
        if (item->HasPVRChannelInfoTag())
          object["type"] = "channel";
        else if (item->HasPVRRecordingInfoTag())
          object["type"] = "recording";
        else if (item->HasMusicInfoTag())
        {
          std::string type = item->GetMusicInfoTag()->GetType();
          if (type == MediaTypeAlbum || type == MediaTypeSong || type == MediaTypeArtist)
            object["type"] = type;
          else if (!item->IsFolder())
            object["type"] = MediaTypeSong;
        }
        else if (item->HasVideoInfoTag() && !item->GetVideoInfoTag()->m_type.empty())
        {
          std::string type = item->GetVideoInfoTag()->m_type;
          if (type == MediaTypeMovie || type == MediaTypeTvShow || type == MediaTypeEpisode ||
              type == MediaTypeMusicVideo)
            object["type"] = type;
        }
        else if (item->HasPictureInfoTag())
          object["type"] = "picture";

        if (!object.isMember("type"))
          object["type"] = "unknown";

        if (fields.contains("fileType"))
          object["fileType"] = item->IsFolder() ? "directory" : "file";
      }
    }

    std::unique_ptr<CThumbLoader> ownLoader;
    if (!thumbLoader)
    {
      ownLoader = ThumbLoaderFor(*item);
      thumbLoader = ownLoader.get();
    }

    if (item->HasPVRChannelInfoTag())
      FillDetails(item->GetPVRChannelInfoTag().get(), item, fields, object, thumbLoader);
    if (item->HasPVRChannelGroupMemberInfoTag())
      FillDetails(item->GetPVRChannelGroupMemberInfoTag().get(), item, fields, object, thumbLoader);
    if (item->HasEPGInfoTag())
      FillDetails(item->GetEPGInfoTag().get(), item, fields, object, thumbLoader);
    if (item->HasPVRRecordingInfoTag())
      FillDetails(item->GetPVRRecordingInfoTag().get(), item, fields, object, thumbLoader);
    if (item->HasPVRTimerInfoTag())
      FillDetails(item->GetPVRTimerInfoTag().get(), item, fields, object, thumbLoader);
    if (item->HasVideoInfoTag())
      FillDetails(item->GetVideoInfoTag(), item, fields, object, thumbLoader);
    if (item->HasMusicInfoTag())
      FillDetails(item->GetMusicInfoTag(), item, fields, object, thumbLoader);
    if (item->HasPictureInfoTag())
      FillDetails(item->GetPictureInfoTag(), item, fields, object, thumbLoader);

    FillDetails(item.get(), item, fields, object, thumbLoader);

    object["label"] = item->GetLabel();
  }
  else
    object = CVariant(CVariant::VariantTypeNull);

  if (resultname)
  {
    if (append)
      result[resultname].append(object);
    else
      result[resultname] = object;
  }
}

bool CFileItemHandler::ApplyFilter(const CVariant& filter,
                                   std::span<const FilterField> fields,
                                   const std::string& rulesType,
                                   CDbUrl& url)
{
  for (const FilterField& field : fields)
  {
    if (!filter.isMember(field.name))
      continue;

    if (field.number)
      url.AddOption(field.option, static_cast<int>(filter[field.name].asInteger()));
    else
      url.AddOption(field.option, filter[field.name].asString());
    return true;
  }

  if (!filter.isObject())
    return true;

  std::string xsp;
  if (!GetXspFiltering(rulesType, filter, xsp))
    return false;

  url.AddOption("xsp", xsp);
  return true;
}

bool CFileItemHandler::FillFileItemList(const CVariant& parameterObject, CFileItemList& list)
{
  CAudioLibrary::FillFileItemList(parameterObject, list);
  CVideoLibrary::FillFileItemList(parameterObject, list);
  CFileOperations::FillFileItemList(parameterObject, list);

  std::string file = parameterObject["file"].asString();
  if (!file.empty() &&
      (URIUtils::IsURL(file) || (CFileUtils::Exists(file) && !CDirectory::Exists(file))))
  {
    bool added = false;
    for (int index = 0; index < list.Size(); index++)
    {
      if (list[index]->GetDynPath() == file || list[index]->GetMusicInfoTag()->GetURL() == file ||
          list[index]->GetVideoInfoTag()->GetPath() == file)
      {
        added = true;
        break;
      }
    }

    if (!added)
    {
      CFileItemPtr item = std::make_shared<CFileItem>(file, false);
      if (item->IsPicture())
      {
        CPictureInfoTag picture;
        picture.Load(item->GetPath());
        *item->GetPictureInfoTag() = picture;
      }
      if (item->GetLabel().empty())
      {
        item->SetLabel(CUtil::GetTitleFromPath(file, false));
        if (item->GetLabel().empty())
          item->SetLabel(URIUtils::GetFileName(file));
      }
      list.Add(item);
    }
  }

  return (list.Size() > 0);
}

JSONRPC_STATUS CFileItemHandler::DiagnoseUnresolvedItem(const CVariant& item, CVariant& result)
{
  const std::string file{item["file"].asString()};
  if (!file.empty() && !URIUtils::IsURL(file) && !CFileUtils::Exists(file, false))
  {
    // A directory named as a file is a malformed request, not a reference that has gone stale
    if (XFILE::CDirectory::Exists(file, false))
      return Fail(result, InvalidParams, Reason::NotAFile, Target("file", item["file"]));
    return Fail(result, NotFound, Reason::NoSuchPath, Target("file", item["file"]));
  }

  const std::string directory{item["directory"].asString()};
  if (!directory.empty() && !XFILE::CDirectory::Exists(directory, false))
    return Fail(result, NotFound, Reason::NoSuchPath, Target("directory", item["directory"]));

  for (const std::string& identifier : LibraryIdentifiers())
  {
    if (item[identifier].asInteger(-1) > 0)
      return Fail(result, NotFound, Reason::NoSuchItem, Target(identifier, item[identifier]));
  }

  return Fail(result, InvalidParams, Reason::NotPlayable);
}

JSONRPC_STATUS CFileItemHandler::CheckAgainstType(const char* type,
                                                  const char* parameter,
                                                  const CVariant& value,
                                                  CVariant& checked,
                                                  CVariant& errorData)
{
  const JSONSchemaTypeDefinitionPtr definition{CJSONServiceDescription::GetType(type)};
  if (!definition)
    return InternalError;

  CVariant data;
  const JSONRPC_STATUS status{definition->Check(value, checked, data)};
  if (status != OK)
  {
    errorData = data;
    errorData["name"] = parameter;
  }
  return status;
}

JSONRPC_STATUS CFileItemHandler::RefuseForKind(const char* parameter,
                                               const std::string& kind,
                                               CVariant& errorData)
{
  errorData = CVariant(CVariant::VariantTypeObject);
  errorData["name"] = parameter;
  errorData["message"] = StringUtils::Format("Not accepted for a {}", kind);
  return InvalidParams;
}

void CFileItemHandler::RenameList(CVariant& result, const char* from, const char* to)
{
  CVariant list{CVariant::VariantTypeArray};
  if (result.isMember(from))
  {
    if (result[from].isArray())
      list = std::move(result[from]);
    result.erase(from);
  }
  result[to] = std::move(list);
}

CVariant CFileItemHandler::GivenMembers(const CVariant& object)
{
  CVariant given{CVariant::VariantTypeObject};
  for (auto member = object.begin_map(); member != object.end_map(); ++member)
  {
    if (!member->second.isNull())
      given[member->first] = member->second;
  }
  return given;
}

CVariant CFileItemHandler::ReadableNames(const CVariant& values, const char* fieldsType)
{
  CVariant names{CVariant::VariantTypeArray};
  const JSONSchemaTypeDefinitionPtr fields{CJSONServiceDescription::GetType(fieldsType)};
  if (!fields || !fields->items)
    return names;

  const std::vector<CVariant>& readable{fields->items->enums};
  for (auto value = values.begin_map(); value != values.end_map(); ++value)
  {
    if (!value->second.isNull() &&
        std::ranges::find(readable, CVariant{value->first}) != readable.end())
      names.push_back(value->first);
  }
  return names;
}

void CFileItemHandler::AnnounceChange(ANNOUNCEMENT::AnnouncementFlag library,
                                      const std::string& kind,
                                      int id,
                                      const CVariant& names,
                                      const CVariant& item)
{
  if (names.empty())
    return;

  CVariant data{CVariant::VariantTypeObject};
  data["type"] = kind;
  data["id"] = id;
  data["properties"] = CVariant{CVariant::VariantTypeObject};
  for (auto name = names.begin_array(); name != names.end_array(); ++name)
  {
    if (item.isMember(name->asString()))
      data["properties"][name->asString()] = item[name->asString()];
  }
  CServiceBroker::GetAnnouncementManager()->Announce(library, "OnUpdate", data);
}

void CFileItemHandler::Sort(CFileItemList& items, const CVariant& parameterObject)
{
  SortDescription sorting;
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return;

  items.Sort(sorting);
}
