/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "FileSystemDataManager.h"
#include "FileSystemDatabaseManagerVersion001.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "ResultStatement.h"
#include "SchemaVersion001.h"
#include "TestHelpers.h"
#include "gtest/gtest.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Array.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

namespace mozilla::dom::fs::test {

using data::FileSystemDatabaseManagerVersion001;
using data::FileSystemFileManager;

// This is a minimal mock  to allow us to safely call the lock methods
// while avoiding assertions
class MockFileSystemDataManager final : public data::FileSystemDataManager {
 public:
  MockFileSystemDataManager(const quota::OriginMetadata& aOriginMetadata,
                            MovingNotNull<nsCOMPtr<nsIEventTarget>> aIOTarget,
                            MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue)
      : FileSystemDataManager(aOriginMetadata, std::move(aIOTarget),
                              std::move(aIOTaskQueue)) {}

  virtual ~MockFileSystemDataManager() {
    // Need to avoid assertions
    mState = State::Closed;
  }
};

static void MakeDatabaseManagerVersion001(
    RefPtr<MockFileSystemDataManager>& aDataManager,
    FileSystemDatabaseManagerVersion001*& aDatabaseManager) {
  TEST_TRY_UNWRAP(auto storageService,
                  MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<mozIStorageService>,
                                          MOZ_SELECT_OVERLOAD(do_GetService),
                                          MOZ_STORAGE_SERVICE_CONTRACTID));

  const auto flags = mozIStorageService::CONNECTION_DEFAULT;
  ResultConnection connection;

  nsresult rv = storageService->OpenSpecialDatabase(kMozStorageMemoryStorageKey,
                                                    VoidCString(), flags,
                                                    getter_AddRefs(connection));
  ASSERT_NSEQ(NS_OK, rv);

  const Origin& testOrigin = GetTestOrigin();

  TEST_TRY_UNWRAP(
      DatabaseVersion version,
      SchemaVersion001::InitializeConnection(connection, testOrigin));
  ASSERT_EQ(1, version);

  nsCOMPtr<nsIFile> testPath;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(testPath));
  ASSERT_NSEQ(NS_OK, rv);

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

  auto fmRes =
      FileSystemFileManager::CreateFileSystemFileManager(std::move(testPath));
  ASSERT_FALSE(fmRes.isErr());

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID),
                QM_VOID);

  quota::OriginMetadata originMetadata = GetTestOriginMetadata();

  nsCString taskQueueName("OPFS "_ns + originMetadata.mOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(do_AddRef(streamTransportService), taskQueueName.get());

  aDataManager = MakeRefPtr<MockFileSystemDataManager>(
      originMetadata, WrapMovingNotNull(streamTransportService),
      WrapMovingNotNull(ioTaskQueue));

  aDatabaseManager = new FileSystemDatabaseManagerVersion001(
      aDataManager, std::move(connection),
      MakeUnique<FileSystemFileManager>(fmRes.unwrap()), rootId);
}

TEST(TestFileSystemDatabaseManagerVersion001, smokeTestCreateRemoveDirectories)
{
  nsresult rv = NS_OK;
  // Ensure that FileSystemDataManager lives for the lifetime of the test
  RefPtr<MockFileSystemDataManager> dataManager;
  FileSystemDatabaseManagerVersion001* rdm = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeDatabaseManagerVersion001(dataManager, rdm));
  UniquePtr<FileSystemDatabaseManagerVersion001> dm(rdm);
  // if any of these exit early, we have to close
  auto autoClose = MakeScopeExit([rdm] { rdm->Close(); });

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

  FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

  TEST_TRY_UNWRAP(EntryId firstChild,
                  dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));

  int32_t dbVersion = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                  dm->GetDirectoryEntries(rootId, dbVersion));
  ASSERT_EQ(1u, entries.directories().Length());
  ASSERT_EQ(0u, entries.files().Length());

  const auto& firstItemRef = entries.directories()[0];
  ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
  << firstItemRef.entryName();
  ASSERT_EQ(firstChild, firstItemRef.entryId());

  TEST_TRY_UNWRAP(EntryId firstChildClone,
                  dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));
  ASSERT_EQ(firstChild, firstChildClone);

  FileSystemChildMetadata secondChildMeta(firstChild, u"Second"_ns);
  TEST_TRY_UNWRAP(EntryId secondChild,
                  dm->GetOrCreateDirectory(secondChildMeta, /* create */ true));

  FileSystemEntryPair shortPair(firstChild, secondChild);
  TEST_TRY_UNWRAP(Path shortPath, dm->Resolve(shortPair));
  ASSERT_EQ(1u, shortPath.Length());
  ASSERT_EQ(u"Second"_ns, shortPath[0]);

  FileSystemEntryPair longPair(rootId, secondChild);
  TEST_TRY_UNWRAP(Path longPath, dm->Resolve(longPair));
  ASSERT_EQ(2u, longPath.Length());
  ASSERT_EQ(u"First"_ns, longPath[0]);
  ASSERT_EQ(u"Second"_ns, longPath[1]);

  FileSystemEntryPair wrongPair(secondChild, rootId);
  TEST_TRY_UNWRAP(Path emptyPath, dm->Resolve(wrongPair));
  ASSERT_TRUE(emptyPath.IsEmpty());

  PageNumber page = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing fEntries,
                  dm->GetDirectoryEntries(firstChild, page));
  ASSERT_EQ(1u, fEntries.directories().Length());
  ASSERT_EQ(0u, fEntries.files().Length());

  const auto& secItemRef = fEntries.directories()[0];
  ASSERT_TRUE(u"Second"_ns == secItemRef.entryName())
  << secItemRef.entryName();
  ASSERT_EQ(secondChild, secItemRef.entryId());

  TEST_TRY_UNWRAP_ERR(
      rv, dm->RemoveDirectory(firstChildMeta, /* recursive */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);

  TEST_TRY_UNWRAP(bool isDeleted,
                  dm->RemoveDirectory(firstChildMeta, /* recursive */ true));
  ASSERT_TRUE(isDeleted);

  FileSystemChildMetadata thirdChildMeta(secondChild, u"Second"_ns);
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(thirdChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  dm->Close();
}

TEST(TestFileSystemDatabaseManagerVersion001, smokeTestCreateRemoveFiles)
{
  nsresult rv = NS_OK;
  // Ensure that FileSystemDataManager lives for the lifetime of the test
  RefPtr<MockFileSystemDataManager> datamanager;
  FileSystemDatabaseManagerVersion001* rdm = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeDatabaseManagerVersion001(datamanager, rdm));
  UniquePtr<FileSystemDatabaseManagerVersion001> dm(rdm);

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

  FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
  // If creating is not allowed, getting a file from empty root fails
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

  // Creating a file under empty root succeeds
  TEST_TRY_UNWRAP(EntryId firstChild,
                  dm->GetOrCreateFile(firstChildMeta, /* create */ true));

  // Second time, the same file is returned
  TEST_TRY_UNWRAP(EntryId firstChildClone,
                  dm->GetOrCreateFile(firstChildMeta, /* create */ true));
  ASSERT_STREQ(firstChild.get(), firstChildClone.get());

  // Directory listing returns the created file
  PageNumber page = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                  dm->GetDirectoryEntries(rootId, page));
  ASSERT_EQ(0u, entries.directories().Length());
  ASSERT_EQ(1u, entries.files().Length());

  auto& firstItemRef = entries.files()[0];
  ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
  << firstItemRef.entryName();
  ASSERT_STREQ(firstChild.get(), firstItemRef.entryId().get());

  nsString type;
  TimeStamp lastModifiedMilliSeconds;
  Path path;
  nsCOMPtr<nsIFile> file;
  rv = dm->GetFile(firstItemRef.entryId(), type, lastModifiedMilliSeconds, path,
                   file);
  ASSERT_NSEQ(NS_OK, rv);

  ASSERT_TRUE(type.IsEmpty());

  const int64_t nowMilliSeconds = PR_Now() / 1000;
  ASSERT_GE(nowMilliSeconds, lastModifiedMilliSeconds);
  const int64_t expectedMaxDelayMilliSeconds = 100;
  const int64_t actualDelay = nowMilliSeconds - lastModifiedMilliSeconds;
  ASSERT_LT(actualDelay, expectedMaxDelayMilliSeconds);

  ASSERT_EQ(1u, path.Length());
  ASSERT_STREQ(u"First"_ns, path[0]);

  ASSERT_NE(nullptr, file);

  // Getting the file entry as directory fails
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

  // Getting or creating the file entry as directory also fails
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

  // Creating a file with non existing parent hash fails

  EntryId notAChildHash = "0123456789abcdef0123456789abcdef"_ns;
  FileSystemChildMetadata notAChildMeta(notAChildHash, u"Dummy"_ns);
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(notAChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  // We create a directory under root
  FileSystemChildMetadata secondChildMeta(rootId, u"Second"_ns);
  TEST_TRY_UNWRAP(EntryId secondChild,
                  dm->GetOrCreateDirectory(secondChildMeta, /* create */ true));

  // The root should now contain the existing file and the new directory
  TEST_TRY_UNWRAP(FileSystemDirectoryListing fEntries,
                  dm->GetDirectoryEntries(rootId, page));
  ASSERT_EQ(1u, fEntries.directories().Length());
  ASSERT_EQ(1u, fEntries.files().Length());

  const auto& secItemRef = fEntries.directories()[0];
  ASSERT_TRUE(u"Second"_ns == secItemRef.entryName())
  << secItemRef.entryName();
  ASSERT_EQ(secondChild, secItemRef.entryId());

  // Create a file under the new directory
  FileSystemChildMetadata thirdChildMeta(secondChild, u"Third"_ns);
  TEST_TRY_UNWRAP(EntryId thirdChild,
                  dm->GetOrCreateFile(thirdChildMeta, /* create */ true));

  FileSystemEntryPair entryPair(rootId, thirdChild);
  TEST_TRY_UNWRAP(Path entryPath, dm->Resolve(entryPair));
  ASSERT_EQ(2u, entryPath.Length());
  ASSERT_EQ(u"Second"_ns, entryPath[0]);
  ASSERT_EQ(u"Third"_ns, entryPath[1]);

  // If recursion is not allowed, the non-empty new directory may not be removed
  TEST_TRY_UNWRAP_ERR(
      rv, dm->RemoveDirectory(secondChildMeta, /* recursive */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);

  // If recursion is allowed, the new directory goes away.
  TEST_TRY_UNWRAP(bool isDeleted,
                  dm->RemoveDirectory(secondChildMeta, /* recursive */ true));
  ASSERT_TRUE(isDeleted);

  // The file under the removed directory is no longer accessible.
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(thirdChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  // The deletion is reflected by the root directory listing
  TEST_TRY_UNWRAP(FileSystemDirectoryListing nEntries,
                  dm->GetDirectoryEntries(rootId, 0));
  ASSERT_EQ(0u, nEntries.directories().Length());
  ASSERT_EQ(1u, nEntries.files().Length());

  const auto& fileItemRef = nEntries.files()[0];
  ASSERT_TRUE(u"First"_ns == fileItemRef.entryName())
  << fileItemRef.entryName();
  ASSERT_EQ(firstChild, fileItemRef.entryId());

  dm->Close();
}

TEST(TestFileSystemDatabaseManagerVersion001, smokeTestCreateMoveDirectories)
{
  // Ensure that FileSystemDataManager lives for the lifetime of the test
  RefPtr<MockFileSystemDataManager> datamanager;
  FileSystemDatabaseManagerVersion001* rdm = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeDatabaseManagerVersion001(datamanager, rdm));
  UniquePtr<FileSystemDatabaseManagerVersion001> dm(rdm);
  auto closeAtExit = MakeScopeExit([&dm]() { dm->Close(); });

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

  FileSystemEntryMetadata rootMeta{rootId, u"root"_ns, /* is directory */ true};

  {
    // Sanity check: no existing items should be found
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(rootId, /* page */ 0u));
    ASSERT_TRUE(contents.directories().IsEmpty());
    ASSERT_TRUE(contents.files().IsEmpty());
  }

  // Create subdirectory
  FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
  TEST_TRY_UNWRAP(EntryId firstChildDir,
                  dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));

  {
    // Check that directory listing is as expected
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(rootId, /* page */ 0u));
    ASSERT_TRUE(contents.files().IsEmpty());
    ASSERT_EQ(1u, contents.directories().Length());
    ASSERT_STREQ(firstChildMeta.childName(),
                 contents.directories()[0].entryName());
  }

  {
    // Try to move subdirectory to its current location
    FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{rootId, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to move subdirectory under itself
    FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{src.entryId(), src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
  }

  {
    // Try to move root under its subdirectory
    FileSystemEntryMetadata src{rootId, rootMeta.entryName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDir, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
  }

  // Create subsubdirectory
  FileSystemChildMetadata firstChildDescendantMeta(firstChildDir,
                                                   u"Descendant"_ns);
  TEST_TRY_UNWRAP(EntryId firstChildDescendant,
                  dm->GetOrCreateDirectory(firstChildDescendantMeta,
                                           /* create */ true));

  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(firstChildDir, /* page */ 0u));
    ASSERT_TRUE(contents.files().IsEmpty());
    ASSERT_EQ(1u, contents.directories().Length());
    ASSERT_STREQ(firstChildDescendantMeta.childName(),
                 contents.directories()[0].entryName());

    TEST_TRY_UNWRAP(Path subSubDirPath,
                    dm->Resolve({rootId, contents.directories()[0].entryId()}));
    ASSERT_EQ(2u, subSubDirPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), subSubDirPath[0]);
    ASSERT_STREQ(firstChildDescendantMeta.childName(), subSubDirPath[1]);
  }

  {
    // Try to move subsubdirectory to its current location
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDir, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to move subsubdirectory under itself
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{src.entryId(), src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
  }

  {
    // Try to move subdirectory under its descendant
    FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDescendant, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
  }

  {
    // Try to move root under its subsubdirectory
    FileSystemEntryMetadata src{rootId, rootMeta.entryName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDescendant, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
  }

  // Create file in the subdirectory with already existing subsubdirectory
  FileSystemChildMetadata testFileMeta(firstChildDir, u"Subfile"_ns);
  TEST_TRY_UNWRAP(EntryId testFile,
                  dm->GetOrCreateFile(testFileMeta, /* create */ true));

  // Get handles to the original locations of the entries
  FileSystemEntryMetadata subSubDir;
  FileSystemEntryMetadata subSubFile;

  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(firstChildDir, /* page */ 0u));
    ASSERT_EQ(1u, contents.files().Length());
    ASSERT_EQ(1u, contents.directories().Length());

    subSubDir = contents.directories()[0];
    ASSERT_STREQ(firstChildDescendantMeta.childName(), subSubDir.entryName());

    subSubFile = contents.files()[0];
    ASSERT_STREQ(testFileMeta.childName(), subSubFile.entryName());
  }

  {
    TEST_TRY_UNWRAP(Path entryPath,
                    dm->Resolve({rootId, subSubFile.entryId()}));
    ASSERT_EQ(2u, entryPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
  }

  {
    // Try to move file to its current location with correct isDirectory flag
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{firstChildDir, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to move file to its current location with incorrect isDirectory flag
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDir, src.entryName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to rename file to a directory with correct isDirectory flag
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ false};
    const FileSystemChildMetadata& dest = firstChildDescendantMeta;
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to rename file to a directory with incorrect isDirectory flag
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ true};
    const FileSystemChildMetadata& dest = firstChildDescendantMeta;
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to rename directory to a file with correct isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    const FileSystemChildMetadata& dest = testFileMeta;
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to rename directory to a file with incorrect isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ false};
    const FileSystemChildMetadata& dest = testFileMeta;
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to move subsubdirectory under a file with correct isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{testFile,
                                 firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);
  }

  {
    // Try to move subsubdirectory under a file with incorrect isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{testFile,
                                 firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);
  }

  {
    // Try to move subsubdirectory under a file with incorrect isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{testFile,
                                 firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);
  }

  {
    // Move file one level up with correct isDirectory flag
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{rootId, src.entryName()};
    TEST_TRY_UNWRAP(bool isMoved, dm->MoveEntry(src, dest));
    ASSERT_TRUE(isMoved);
  }

  {
    // Check that listings are as expected
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(firstChildDir, 0u));
    ASSERT_TRUE(contents.files().IsEmpty());
    ASSERT_EQ(1u, contents.directories().Length());
    ASSERT_STREQ(firstChildDescendantMeta.childName(),
                 contents.directories()[0].entryName());
  }

  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(rootId, 0u));
    ASSERT_EQ(1u, contents.files().Length());
    ASSERT_EQ(1u, contents.files().Length());
    ASSERT_STREQ(testFileMeta.childName(), contents.files()[0].entryName());
  }

  {
    TEST_TRY_UNWRAP(Path entryPath,
                    dm->Resolve({rootId, subSubFile.entryId()}));
    ASSERT_EQ(1u, entryPath.Length());
    ASSERT_STREQ(testFileMeta.childName(), entryPath[0]);
  }

  {
    // Try to get a handle to the old item
    TEST_TRY_UNWRAP_ERR(nsresult rv,
                        dm->GetOrCreateFile(testFileMeta, /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
  }

  {
    // Try to move + rename file one level down to collide with subSubDirectory
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{firstChildDir,
                                 firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  // Rename file first and then try to move it to collide with subSubDirectory
  {
    // Rename
    FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{rootId, firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP(bool isMoved, dm->MoveEntry(src, dest));
    ASSERT_TRUE(isMoved);
  }

  {
    // Try to move one level down
    FileSystemEntryMetadata src{testFile, firstChildDescendantMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{firstChildDir,
                                 firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  {
    // Try to move subSubDirectory one level up to collide with file
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{rootId, firstChildDescendantMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  // Create a new file in the subsubdirectory
  FileSystemChildMetadata newFileMeta{firstChildDescendant,
                                      testFileMeta.childName()};
  TEST_TRY_UNWRAP(EntryId newFile,
                  dm->GetOrCreateFile(newFileMeta, /* create */ true));

  {
    TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
    ASSERT_EQ(3u, entryPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
    ASSERT_STREQ(firstChildDescendantMeta.childName(), entryPath[1]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[2]);
  }

  {
    // Move subSubDirectory one level up and rename it to testFile's old name
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{rootId, testFileMeta.childName()};
    TEST_TRY_UNWRAP(bool isMoved, dm->MoveEntry(src, dest));
    ASSERT_TRUE(isMoved);
  }

  {
    // Try to get handles to the moved items
    TEST_TRY_UNWRAP_ERR(nsresult rv,
                        dm->GetOrCreateDirectory(firstChildDescendantMeta,
                                                 /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    // Still under the same parent
    TEST_TRY_UNWRAP(EntryId handle, dm->GetOrCreateFile(newFileMeta,
                                                        /* create */ false));
    ASSERT_STREQ(handle, newFile);

    TEST_TRY_UNWRAP(handle,
                    dm->GetOrCreateDirectory({rootId, testFileMeta.childName()},
                                             /* create */ false));
    ASSERT_STREQ(handle, firstChildDescendant);
  }

  {
    // Check that new file path is as expected
    TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
    ASSERT_EQ(2u, entryPath.Length());
    ASSERT_STREQ(testFileMeta.childName(), entryPath[0]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
  }

  {
    // Try to overwrite subDirectory with subSubDirectory with rename
    FileSystemEntryMetadata src{firstChildDescendant,
                                firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{rootId, firstChildMeta.childName()};
    TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
    ASSERT_NSEQ(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, rv);
  }

  // Move first file and subSubDirectory back one level down keeping the names
  {
    // First file with wrong isDirectory flag
    FileSystemEntryMetadata src{testFile, firstChildDescendantMeta.childName(),
                                /* is directory */ true};
    FileSystemChildMetadata dest{firstChildDir,
                                 firstChildDescendantMeta.childName()};

    // Flag is ignored
    TEST_TRY_UNWRAP(bool isMoved, dm->MoveEntry(src, dest));
    ASSERT_TRUE(isMoved);
  }

  {
    // Then move the directory with wrong isDirectory flag
    FileSystemEntryMetadata src{firstChildDescendant, testFileMeta.childName(),
                                /* is directory */ false};
    FileSystemChildMetadata dest{firstChildDir, testFileMeta.childName()};

    // Flag is ignored
    TEST_TRY_UNWRAP(bool isMoved, dm->MoveEntry(src, dest));
    ASSERT_TRUE(isMoved);
  }

  // Check that listings are as expected
  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(rootId, 0u));
    ASSERT_TRUE(contents.files().IsEmpty());
    ASSERT_EQ(1u, contents.directories().Length());
    ASSERT_STREQ(firstChildMeta.childName(),
                 contents.directories()[0].entryName());
  }

  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(firstChildDir, 0u));
    ASSERT_EQ(1u, contents.files().Length());
    ASSERT_EQ(1u, contents.directories().Length());
    ASSERT_STREQ(firstChildDescendantMeta.childName(),
                 contents.files()[0].entryName());
    ASSERT_STREQ(testFileMeta.childName(),
                 contents.directories()[0].entryName());
  }

  {
    TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                    dm->GetDirectoryEntries(firstChildDescendant, 0u));
    ASSERT_EQ(1u, contents.files().Length());
    ASSERT_TRUE(contents.directories().IsEmpty());
    ASSERT_STREQ(testFileMeta.childName(), contents.files()[0].entryName());
  }

  // Names are swapped
  {
    TEST_TRY_UNWRAP(Path entryPath,
                    dm->Resolve({rootId, subSubFile.entryId()}));
    ASSERT_EQ(2u, entryPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
    ASSERT_STREQ(firstChildDescendantMeta.childName(), entryPath[1]);
  }

  {
    TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, subSubDir.entryId()}));
    ASSERT_EQ(2u, entryPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
  }

  {
    // Check that new file path is also as expected
    TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
    ASSERT_EQ(3u, entryPath.Length());
    ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
    ASSERT_STREQ(testFileMeta.childName(), entryPath[2]);
  }

  {
    // Try to get handles to the old items
    TEST_TRY_UNWRAP_ERR(nsresult rv,
                        dm->GetOrCreateFile({rootId, testFileMeta.childName()},
                                            /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateFile({rootId, firstChildDescendantMeta.childName()},
                                /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateDirectory({rootId, testFileMeta.childName()},
                                     /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    TEST_TRY_UNWRAP_ERR(rv, dm->GetOrCreateDirectory(
                                {rootId, firstChildDescendantMeta.childName()},
                                /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateFile({firstChildDir, testFileMeta.childName()},
                                /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateDirectory(
                {firstChildDir, firstChildDescendantMeta.childName()},
                /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

    TEST_TRY_UNWRAP_ERR(rv,
                        dm->GetOrCreateFile({testFile, newFileMeta.childName()},
                                            /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
  }
}

}  // namespace mozilla::dom::fs::test
