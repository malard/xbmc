# Migrating a client from JSON-RPC 13 to 14

Version 14 is a breaking release: a version 13 client is not guaranteed to
work against it unchanged. Everything that breaks is listed here, with what to
do about it.

The baseline is **13.5.0, the version Kodi 21 (Omega) shipped** — the last
version delivered in a stable release. If you tested against a Kodi 22
pre-release you were on 13.8.0 or 13.11.0, which already carry some of the
additions in the [changelog](CHANGELOG.md) but none of the breaks below.

Check what you are talking to before you assume either shape:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Version"}
```

```json
{"jsonrpc": "2.0", "id": 1, "result": {"version": {"major": 14, "minor": 0, "patch": 0}}}
```

Nothing below depends on Kodi's own version. A client that supports both
should branch on `version.major`.

---

## 1. Errors are more specific than `InvalidParams`

**Most likely to break you, and the easiest to miss**, because the calls
still work — only the failures changed.

Version 13 answered `-32602 InvalidParams` for almost everything that went
wrong after the parameters had been validated. A client could not tell "you
asked for a movie that is not in the library" from "you sent a malformed
request". Version 14 separates them:

| Code | Name | Means |
|---|---|---|
| -32602 | `InvalidParams` | the request itself is wrong |
| -32098 | `NotFound` | what you named does not exist |
| -32097 | `Unavailable` | it exists but cannot be provided now |
| -32096 | `AccessDenied` | the path is not in a source shared for remote access |
| -32603 | `InternalError` | the method failed for a reason none of the above describes |

Affected: `Files.GetDirectory`, `Files.GetFileDetails`,
`Files.SetFileDetails`, `Files.PrepareDownload`, `Files.Download`,
`Player.Open`, `VideoLibrary.Scan`, `VideoLibrary.Clean`,
`AudioLibrary.GetArtistDetails`, `Settings.GetSettingValue`,
`Settings.SetSettingValue`, `Settings.ResetSettingValue`.

The three `Settings` calls also stop refusing a *hidden* setting: a setting
that `advancedsettings.xml` pins, or that hidden-value loading read, is now
read and written like any other. If you relied on `InvalidParams` to mean
"hidden on this installation", read `enabled` from `Settings.GetSettings`
instead - a write is refused, as `Unavailable`, only when the setting is
disabled by its dependencies.

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

Version 14 says what happened:

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

## 5. `XBMC.GetInfoLabels` and `XBMC.GetInfoBooleans` are deprecated

They still work and are the same implementation, but **everything deprecated
in 14 is removed in 15**, so this is work to do before the next major version
rather than something to revisit later.

```diff
- {"method": "XBMC.GetInfoLabels", "params": {"labels": ["System.Time"]}}
+ {"method": "GUI.GetInfoLabels",  "params": {"labels": ["System.Time"]}}
```

Parameters, result and required permission are identical, so this is a rename
at the call site and nothing more.

Note also that both methods describe their result correctly for the first
time: `GetInfoBooleans` returns an object of **booleans**. The schema said
strings; the wire has always carried booleans. If you generate code from the
schema, your generated type changes even though the traffic does not.

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

Superseded by `season` and `episode`. The old names still work, and go in 15
along with everything else deprecated here.

They have carried "Deprecated" in their description since 13.6.0, but only in
prose, and 13.6.0 never reached a stable release — so if you are coming from
Kodi 21 this is the first you will have seen of it. They now carry the
`deprecated` annotation, which means you can find them from
`JSONRPC.Introspect` or `openrpc.json` rather than by reading descriptions.

## 8. `Files.GetDirectory` browses directories, and answers `properties`

Three changes to one call, all of them in the direction of it doing what its
name says.

**A folder stays a folder.** Version 13 matched each entry against the video
library and, on a hit, replaced the entry with the library item — path and
all. For the common layout of one movie per folder, that turned every scanned
folder into the movie inside it:

```json
{"file": "smb://nas/Movies/Hail Caesar (2016)/Hail.Caesar.2016.mp4",
 "filetype": "file", "label": "Hail, Caesar!"}
```

Version 14 keeps the entry the caller was browsing and annotates it:

```json
{"file": "smb://nas/Movies/Hail Caesar (2016)/",
 "filetype": "directory", "label": "Hail, Caesar!"}
```

**What to do.** If you followed `file` to play an item, check `filetype`
first — a `directory` is a level to descend into, not something to open. The
change only affects folders; an entry for a file is byte-for-byte what it was.

**`"media": "files"` answers `properties`.** It previously ignored them and
returned bare listings, which is why a client that wanted artwork had to ask
for `"media": "video"` and accept the folder flattening above. Both modes now
return the same details for the same entry, so the workaround is no longer
needed and `"media": "files"` is the mode to browse with.

A request that names no `properties` still gets the plain listing it always
got, and still costs no library lookups — so a client walking a tree for its
structure alone pays nothing for this.

**Tv show folders resolve.** Version 13 looked up movies, episodes and music
videos, so a show's folder came back with no title, art or `watchedepisodes`
even under `"media": "video"`. It now carries the same details
`VideoLibrary.GetTVShows` reports for that show.

Note that a show has no `thumbnail` — that is true of `VideoLibrary.GetTVShows`
too. Use `art.poster`.

---

## 9. The four `VideoLibrary.Refresh*` methods are deprecated

`RefreshMovie`, `RefreshTVShow`, `RefreshEpisode` and `RefreshMusicVideo` are
superseded by one `VideoLibrary.Refresh` that names the item rather than
encoding its type in the method name. The old names still work, and go in 15
along with everything else deprecated here.

```diff
- {"method": "VideoLibrary.RefreshMovie", "params": {"movieid": 42}}
+ {"method": "VideoLibrary.Refresh",      "params": {"item": {"movieid": 42}}}
```

Unlike the `XBMC.*` renames above this is not only a rename: the id moves
inside an `item` object. `ignorenfo`, `title` and `refreshepisodes` stay where
they are.

The item names exactly one of `movieid`, `setid`, `tvshowid`, `seasonid`,
`episodeid` or `musicvideoid`. A movie set and a season can therefore be
refreshed for the first time — the interface has always been able to, and no
per-type method covered either. `refreshepisodes` applies to a tv show or a
season and is ignored by the rest, as it always was.

## Finding the rest

Anything deprecated is marked `"deprecated": true` on its method or its
schema, so you do not have to take this document's word for the list:

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

- **Playback failure is now reported.** `Player.OnPlaybackFailed` fires when
  playback was requested and did not happen, with a `reason` of `unplayable`,
  `unresolved`, `locked` or `error`. Previously a failed playback produced no
  notification at all.
- **Playlist shuffle and repeat** are readable (`Playlist.GetProperties`),
  settable (`Playlist.SetShuffle`, `Playlist.SetRepeat`) and observable
  (`Playlist.OnPropertyChanged`).
- **Skin lifecycle notifications**: `GUI.OnSkinLoaded`,
  `GUI.OnSkinLoadFailed` and `GUI.OnSkinUnloading`.
- **PVR providers** are listable via `PVR.GetProviders` and
  `PVR.GetProviderDetails`, and `PVR.GetPlayableBroadcasts` answers which
  broadcasts in a time range can be played back.
- **`VideoLibrary.SetSourceContent`** assigns a content type and scraper to a
  source path, which previously only the "Set content" dialog could do.
- **`Player.GetChapters`** returns the playing item's chapters.
- **`GUI.TakeScreenshot`**, `Database.GetDatabaseName`,
  `AudioLibrary.RefreshAlbum` and `AudioLibrary.RefreshArtist`.
- **PVR image properties are URLs** the web server's `/image/` endpoint can
  serve. Previously they were paths that only the machine running Kodi could
  read, so a remote client could not display them.
- **A PVR channel keeps its own logo** in `icon` and `thumbnail` even when the
  programme airing has its own artwork; the programme's artwork is under
  `broadcastnow`.
- **`Player.OnPropertyChanged`** carries every member its declared type
  promises, rather than a subset.
- **New properties**: `stationname`, `episodename` and `episodepart` on list
  items, the parental rating fields on PVR broadcasts and recordings,
  `bitspersample` on an audio stream, `status` and `trailer` on a TV show,
  and `lastlibrarycheck` on a texture.
