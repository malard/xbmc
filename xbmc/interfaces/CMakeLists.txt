set(SOURCES AnnouncementEvents.cpp
            AnnouncementManager.cpp
            PlaybackValues.cpp)

set(HEADERS AnnouncementEvents.h
            AnnouncementManager.h
            AnnouncementMessages.h
            IAnnouncer.h
            PlaybackValues.h)

core_add_library(interfaces)
