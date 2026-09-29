# Migrating a client from JSON-RPC 13 or 14 to 15

Version 15 is a breaking release: a client written against version 13 or 14
is not guaranteed to work against it unchanged. Everything that breaks is
listed here, with what to do about it.

The baseline is **13.5.0, the version Kodi 21 (Omega) shipped** — the last
version delivered in a stable release. The Kodi 22 pre-releases carried
13.8.0 to 13.200.0; 13.200.0 (22.0b2) already has the library-id part of
section 1 and the BCP 47 languages of section 10, and none of the other
breaks.

Check what you are talking to before you assume either shape:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Version"}
```

```json
{"jsonrpc": "2.0", "id": 1, "result": {"version": {"major": 15, "minor": 0, "patch": 0}}}
```

Nothing below depends on Kodi's own version. A client that supports both
should branch on `version.major`.

---

## 1. Errors are more specific than `InvalidParams`

**Most likely to break you, and the easiest to miss**: the calls still work,
only the failures changed.

Version 13 answered `-32602 InvalidParams` for almost everything that went
wrong after the parameters had been validated. Version 15 separates them:

| Code | Name | Means |
|---|---|---|
| -32602 | `InvalidParams` | the request itself is wrong |
| -32098 | `NotFound` | what you named does not exist |
| -32097 | `Unavailable` | it exists but cannot be provided now |
| -32096 | `AccessDenied` | it is locked on this installation: a path outside every shared source, or a setting level behind the profile's lock |
| -32603 | `InternalError` | the method failed for a reason none of the above describes |

Affected since 13.200.0: every `AudioLibrary` and `VideoLibrary`
`Get*Details`, `Set*Details` and `Refresh*` method answers `NotFound` for an
id no item has, and `Player.Open` answers `Unavailable` for an item it cannot
reach. New in 15: `Files.GetDirectory`, `Files.GetFileDetails`,
`Files.SetFileDetails`, `Files.PrepareDownload`, `Files.Download`,
`Player.Open`, `VideoLibrary.Scan`, `VideoLibrary.Clean`,
`AudioLibrary.GetArtistDetails`, `Settings.GetSettingValue`,
`Settings.SetSettingValue`, `Settings.ResetSettingValue`, and every `PVR`
method that takes a channel, channel group, broadcast, timer or recording
id.

The three `Settings` calls also stop refusing a *hidden* setting. If you
relied on `InvalidParams` to mean "hidden on this installation", read
`enabled` from `Settings.GetSettings` instead; a write is refused, as
`Unavailable`, only when the setting is disabled by its dependencies.

**What to do.** If you test `error.code == -32602` to decide that something is
missing, you will now miss the case. Treat -32098, -32097 and -32096 as
failures too, and prefer them for the "gone" and "forbidden" messages you show
a user.

The full taxonomy is discoverable rather than hardcoded:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Introspect",
 "params": {"filter": {"type": "error", "id": "NotFound"}}}
```

---

## 2. `Playlist.Add` and `Playlist.Insert` return a result

Version 13 returned the string `"OK"` whether or not anything was added, and
dropped silently whatever it could not resolve.

```json
{"result": "OK"}
```

Version 15 says what happened:

```json
{"result": {"added": 2,
            "unresolved": [{"item": {"movieid": 4321}, "reason": "notfound"}]}}
```

`reason` is `notfound`, `unavailable` or `invalid`. When *nothing* was added
the call is an error instead — `NotFound` if anything named something real
that has gone, `InvalidParams` if every entry was malformed.

**What to do.** If you check `result == "OK"`, that test now fails against a
successful call. Read `result.added`, and show `result.unresolved` if you
report progress to a user.

---

## 3. A PVR channel's `uniqueid` is now `channeluid`

`PVR.Details.Channel.uniqueid` is gone. The same value is `channeluid`, which
is what a broadcast and a recording already called it.

```diff
- {"channelid": 12, "uniqueid": 8201}
+ {"channelid": 12, "channeluid": 8201}
```

`uniqueid` is no longer a member of `PVR.Fields.Channel`, so requesting it
returns `InvalidParams`.

**What to do.** Rename the field you request and the one you read. Note this
is unrelated to `uniqueid` on a library item, which is a set of scraper
identifiers and has not changed.

---

## 4. An unselected stream is `null`, not `{}`

`Player.GetProperties` reported `currentaudiostream`, `currentvideostream`
and `currentsubtitle` as an empty object when nothing was selected.

```diff
- {"currentsubtitle": {}}
+ {"currentsubtitle": null}
```

**What to do.** Test for `null` before reading `index`. A client that checks
"is this object non-empty" keeps working; one that reads `.index`
unconditionally will now fault on `null`.

`Player.Subtitle` also gains a `codec` member.

---

## 5. `XBMC.GetInfoLabels` and `XBMC.GetInfoBooleans` are removed

The `XBMC` namespace is gone. `GUI.GetInfoLabels` and `GUI.GetInfoBooleans`
are the same methods.

```diff
- {"method": "XBMC.GetInfoLabels", "params": {"labels": ["System.Time"]}}
+ {"method": "GUI.GetInfoLabels",  "params": {"labels": ["System.Time"]}}
```

Parameters, result and required permission are identical, so this is a rename
at the call site and nothing more.

`GetInfoBooleans` now describes its result as an object of **booleans**; the
schema said strings, the wire always carried booleans. A type generated from
the schema changes even though the traffic does not.

---

## 6. `JSONRPC.Introspect` answers in JSON Schema 2020-12

**Only affects you if you consume the service description itself.** A client
that just calls methods is unaffected by this section.

The description was JSON Schema draft-03. It is now 2020-12:

| draft-03 | 2020-12 |
|---|---|
| `"extends": "Name"` | `"allOf": [{"$ref": "#/$defs/Name"}]` |
| `"$ref": "Name"` | `"$ref": "#/$defs/Name"` |
| `"enums": [...]` | `"enum": [...]` |
| `"required": true` on a property | `"required": ["prop"]` on the object |
| `"type": [ {...}, {...} ]` | `"anyOf": [ {...}, {...} ]` |
| `"type": "any"` | the keyword is omitted |
| `{"name": "x", "type": "string"}` as a param | `{"name": "x", "schema": {"type": "string"}}` |

A method's parameters are now content descriptors: `name`, `required` and
`description` belong to the descriptor, and the schema of the value sits
under `schema`. Tuple-form `items`, `additionalItems`, `divisibleBy` and the
boolean `exclusiveMinimum`/`exclusiveMaximum` are no longer read; the shipped
schema never used them.

Two definitions that were inline and named by an `id` are now global types in
their own right: `Notifications.Library.Audio.Type` and
`Notifications.Library.Video.Type`.

**What to do.** If you validate against the description, use a 2020-12
validator. If you generate code from it, most generators support 2020-12
directly and needed a shim for draft-03. You can also skip `Introspect`
entirely and consume [openrpc.json](openrpc.json), which is generated from the
same schema and gated in CI so it cannot drift.

---

## 7. `seasonnum` and `episodenum` on a PVR broadcast are deprecated

Superseded by `season` and `episode`. The old names still work.

Their descriptions have said "Deprecated" since 13.6.0, which reached no
stable release. They now carry the `deprecated` annotation, so
`JSONRPC.Introspect` and `openrpc.json` report it.

## 8. `Files.GetDirectory` browses directories, and answers `properties`

Three changes to one call.

**A folder stays a folder.** Version 13 matched each entry against the video
library and, on a hit, replaced the entry with the library item, path and
all. With one movie per folder, every scanned folder became the movie inside
it:

```json
{"file": "smb://nas/Movies/Hail Caesar (2016)/Hail.Caesar.2016.mp4",
 "filetype": "file", "label": "Hail, Caesar!"}
```

Version 15 keeps the entry the caller was browsing and annotates it:

```json
{"file": "smb://nas/Movies/Hail Caesar (2016)/",
 "filetype": "directory", "label": "Hail, Caesar!"}
```

**What to do.** If you followed `file` to play an item, check `filetype`
first: a `directory` is a level to descend into. Only folders change; an
entry for a file is byte-for-byte what it was.

**`"media": "files"` answers `properties`.** It previously ignored them and
returned bare listings. Both modes now return the same details for the same
entry, so `"media": "files"` is the mode to browse with. A request naming no
`properties`, or only file properties, still gets the plain listing and costs no
library lookups.

**Tv show folders resolve.** Version 13 looked up movies, episodes and music
videos, never shows. A show's folder now carries the details
`VideoLibrary.GetTVShows` reports for it. A show has no `thumbnail`; use
`art.poster`.

---

## 9. The four `VideoLibrary.Refresh*` methods are deprecated

`RefreshMovie`, `RefreshTVShow`, `RefreshEpisode` and `RefreshMusicVideo` are
superseded by one `VideoLibrary.Refresh` that names the item. The old names
still work.

```diff
- {"method": "VideoLibrary.RefreshMovie", "params": {"movieid": 42}}
+ {"method": "VideoLibrary.Refresh",      "params": {"item": {"movieid": 42}}}
```

The id moves inside an `item` object; `ignorenfo`, `title` and
`refreshepisodes` stay where they are. The item names exactly one of
`movieid`, `setid`, `tvshowid`, `seasonid`, `episodeid` or `musicvideoid`, so
a movie set and a season can be refreshed for the first time.
`refreshepisodes` applies to a tv show or a season and is ignored by the rest.

---

## 10. Stream languages are BCP 47 tags

`language` on `Player.Audio.Stream`, `Player.Video.Stream` and
`Player.Subtitle`, and inside a library item's `streamdetails`, is a BCP 47
language tag rather than an ISO 639-2/B code.

```diff
- {"language": "eng"}
+ {"language": "en"}
```

A player stream carries a region when the player knows one, `en-AU`; a
library stream never does. An empty value still means the stream declared
none, and a value Kodi does not recognise passes through unchanged.

**What to do.** Match on the primary subtag (`en` from `en-AU`) rather than
on a three-letter code. Since 13.200.0.

## 11. Playlists are named, and players are addressed by playlist

A playlist is `video`, `audio` or `picture`, in place of the numeric
`playlistid` every `Playlist` method and notification took, and
`Playlist.GetPlaylists` answers the names.

```diff
- {"method": "Playlist.GetItems", "params": {"playlistid": 1}}
+ {"method": "Playlist.GetItems", "params": {"playlist": "video"}}
```

`playerid` is gone from every `Player` method. Version 13 resolved it through
the playlist in use, so it never named a player: playerid 1 was accepted only
while the video playlist was current, and accepted even when nothing played.
A `Player` method now takes an optional `playlist`, `video` or `audio`. With
none, it acts on everything playing, the playback and a slideshow beside it.
A named playlist that nothing is playing through answers `FailedToExecute`.

```diff
- {"method": "Player.Stop", "params": {"playerid": 1}}
+ {"method": "Player.Stop"}
```

`Player.GetActivePlayers` is removed. `Player.GetProperties` reports the
`playlist` being played and the `playertype`, and the `player` object of every
`Player.On*` notification carries `players`, the playlists the playback holds,
in place of `playerid`.

`Playlist.GetItems` answers each entry with its `position` in the list and its
`displayorder`, its place in play order; the two differ while the playlist is
shuffled. `Player.GoTo` and `Playlist.Remove` take a `position`.
`Player.GetProperties` reports `displayorder` beside `position`.

**What to do.** Send playlist names where you sent numbers. Drop `playerid`
from `Player` calls, or send the `playlist` you mean. Read what is playing
from `Player.GetProperties` or from the notification's `players` rather than
from `Player.GetActivePlayers`.

## 12. Settings writes need the `WriteSetting` permission

`Settings.SetSettingValue`, `Settings.ResetSettingValue` and
`Settings.SetSkinSettingValue` have always documented `WriteSetting` as their
permission, but no such permission existed and they were gated at `ReadData`.
An HTTP `GET` request, which carries `ReadData` alone, could change any
setting. The permission now exists, those three methods and
`Settings.SetLevel` require it, and `JSONRPC.Permission` reports it.

**Who is affected.** Only a client that sends a settings write as an HTTP
`GET` request, with the call in the `request` query argument, or as JSONP.
The web server gives a `GET` request `ReadData` alone and a `POST` request
every permission, so a settings write over `POST` is unchanged, and so is
one over TCP, from Python or from the Android interface, all of which hold
every permission.

**What to do.** Send the same request body as an HTTP `POST` to `/jsonrpc`
with `Content-Type: application/json`, as every other write already has to
be. Nothing in the request changes but the transport:

```
GET  /jsonrpc?request={"jsonrpc":"2.0","id":1,"method":"Settings.SetSettingValue","params":{"setting":"lookandfeel.enablerssfeeds","value":false}}
```

answers `BadPermission` (-32099), while

```
POST /jsonrpc
Content-Type: application/json

{"jsonrpc":"2.0","id":1,"method":"Settings.SetSettingValue","params":{"setting":"lookandfeel.enablerssfeeds","value":false}}
```

answers `true` as it always has. A client that checks `JSONRPC.Permission`
first sees `WriteSetting` false on `GET` and true on `POST`.

## 13. The texture cache is under `Application`

A namespace names what it acts on, and the thumbnail cache is Kodi's
internals.

```diff
- {"method": "Textures.GetTextures",    "params": {"properties": ["url"]}}
+ {"method": "Application.GetTextures", "params": {"properties": ["url"]}}
```

`Textures.RemoveTexture` is `Application.RemoveTexture`. Parameters, results
and permissions are unchanged.

## 14. Shuffle and repeat belong to the playlist

`Player.SetShuffle` and `Player.SetRepeat` are removed. `Player.GetProperties`
no longer reports `shuffled` or `repeat`, and `Player.OnPropertiesChanged` no
longer carries them.

```diff
- {"method": "Player.SetShuffle",   "params": {"shuffle": true}}
+ {"method": "Playlist.SetShuffle", "params": {"playlist": "audio", "shuffle": true}}
```

**What to do.** Set shuffle and repeat with `Playlist.SetShuffle` and
`Playlist.SetRepeat`, read them from `Playlist.GetProperties`, and follow
them through `Playlist.OnPropertiesChanged`. The playlist being played is the
`playlist` property of `Player.GetProperties`. A slideshow's shuffle is the
`picture` playlist's.

## 15. Volume belongs to `Player`

`Player` is what is seen and heard now, whether or not anything plays.
`Application.SetVolume` and `Application.SetMute` are removed, and
`Application.GetProperties` no longer reports `volume`, `muted` or
`contentrect`.

```diff
- {"method": "Application.SetVolume",  "params": {"volume": 40}}
+ {"method": "Player.SetProperties",   "params": {"properties": {"volume": 40}}}
- {"method": "Application.SetVolume",  "params": {"volume": "increment"}}
+ {"method": "Player.VolumeUp"}
- {"method": "Application.SetMute",    "params": {"mute": true}}
+ {"method": "Player.SetProperties",   "params": {"properties": {"muted": true}}}
```

`Player.SetProperties` changes only the properties named and answers with
their values now in force. `Player.GetProperties` answers `volume`, `muted`
and `contentrect` with nothing playing; every other property still needs
something to play.

**What to do.** Read volume, mute and `contentrect` from
`Player.GetProperties`. There is no toggle: send the `muted` value you want.

## 16. State changes arrive as `OnPropertiesChanged`

`Player.OnPropertyChanged` and `Playlist.OnPropertyChanged` are renamed
`Player.OnPropertiesChanged` and `Playlist.OnPropertiesChanged`, and what
changed is under `data.properties`, using the names `GetProperties` answers
with. `Application.OnVolumeChanged` is removed.

```diff
- {"method": "Application.OnVolumeChanged", "params": {"data": {"volume": 40, "muted": false}}}
+ {"method": "Player.OnPropertiesChanged",  "params": {"data": {"properties": {"volume": 40}}}}
```

Only what changed is carried: a volume change carries `volume`, a mute
change `muted`. `player` is present when the change belongs to a playback,
and absent for volume and mute, which change whether or not anything plays.

`Player.OnPause`, `Player.OnResume`, `Player.OnSpeedChanged` and
`Player.OnSeek` are removed, because each is only a change of state: pause,
resume and a speed change arrive as `Player.OnPropertiesChanged` carrying
`speed` (0 while paused), and a seek as one carrying `time`.

```diff
- {"method": "Player.OnPause",             "params": {"data": {"item": {...}, "player": {"players": ["video"], "speed": 0}}}}
+ {"method": "Player.OnPropertiesChanged", "params": {"data": {"properties": {"speed": 0}, "player": {"players": ["video"]}}}}
```

**What to do.** Merge `data.properties` into the state you hold for the
player or the playlist, as you would a `GetProperties` answer. Treat
`speed` 0 as paused.

## Finding the rest

Anything deprecated is marked `"deprecated": true` on its method or its
schema:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Introspect",
 "params": {"getdescriptions": false}}
```

The flag is reported even with descriptions suppressed. `openrpc.json` carries
the same flag for offline tooling.

---

## Nothing to do, but worth knowing

New since Kodi 21 and safe to ignore until you want it. The
[changelog](CHANGELOG.md) has the complete list.

- **Playback failure is reported.** `Player.OnPlaybackFailed` fires when
  playback was requested and did not happen, with a `reason` of `unplayable`,
  `unresolved`, `locked` or `error`.
- **Playlist shuffle and repeat** are readable (`Playlist.GetProperties`),
  settable (`Playlist.SetShuffle`, `Playlist.SetRepeat`) and observable
  (`Playlist.OnPropertiesChanged`).
- **Skin lifecycle notifications**: `GUI.OnSkinLoaded`,
  `GUI.OnSkinLoadFailed` and `GUI.OnSkinUnloading`.
- **`PVR.GetPlayableBroadcasts`** answers which broadcasts in a time range can
  be played back.
- **`VideoLibrary.SetSourceContent`** assigns a content type and scraper to a
  source path, which previously only the "Set content" dialog could do.
- **`Player.GetChapters`** returns the playing item's chapters.
- **`GUI.TakeScreenshot`**, `Database.GetDatabaseName`,
  `AudioLibrary.RefreshAlbum` and `AudioLibrary.RefreshArtist`.
- **PVR image properties are URLs** the web server's `/image/` endpoint can
  serve, in place of paths only the machine running Kodi could read.
- **A PVR channel keeps its own logo** in `icon` and `thumbnail` even when the
  programme airing has its own artwork; the programme's artwork is under
  `broadcastnow`.
- **`Player.OnPropertiesChanged`** carries every member its declared type
  promises, rather than a subset.
- **New properties**: `stationname`, `episodename` and `episodepart` on list
  items, the parental rating fields on PVR broadcasts and recordings,
  `bitspersample` on an audio stream, `status` and `trailer` on a TV show,
  and `lastlibrarycheck` on a texture.
