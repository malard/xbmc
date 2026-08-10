set(SOURCES TestDeprecatedMethodSchema.cpp
            TestFileItemHandler.cpp
            TestJSONRPCPermission.cpp
            TestJSONRPCStatus.cpp
            TestJSONServiceDescription.cpp
            TestJSONServiceDescriptionIntrospect.cpp
            TestPVREpgFields.cpp
            TestPVRProviderSchema.cpp
            TestVideoLibrarySetSourceContent.cpp)

core_add_test_library(jsonrpc_test)
