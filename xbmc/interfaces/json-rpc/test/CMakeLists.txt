set(SOURCES TestDeprecatedMethodSchema.cpp
            TestFileItemHandler.cpp
            TestJSONRPCPermission.cpp
            TestJSONRPCStatus.cpp
            TestJSONServiceDescription.cpp
            TestJSONServiceDescriptionIntrospect.cpp
            TestPlayerOpenDirectory.cpp
            TestPVRBroadcastRange.cpp
            TestPVRBroadcastSchema.cpp
            TestPVREpgFields.cpp
            TestPVRProviderSchema.cpp
            TestPVRRecordingSchema.cpp
            TestSettingsLevel.cpp
            TestSharedFieldDeclarations.cpp
            TestVideoLibraryPlaybackState.cpp
            TestVideoLibraryRefreshSchema.cpp
            TestVideoLibrarySetSourceContent.cpp)

core_add_test_library(jsonrpc_test)
