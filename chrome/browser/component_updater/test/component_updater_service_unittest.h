// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_UPDATER_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_UPDATER_SERVICE_UNITTEST_H_

#include <list>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/test/component_patcher_mock.h"
#include "content/public/test/test_notification_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::TestNotificationTracker;

class GURL;
class TestInstaller;

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8 jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                           0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                           0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                           0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};
// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
const uint8 abag_hash[] = {0x01, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                           0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                           0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                           0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x01};
// component 3 has extension id "ihfokbkgjpifnbbojhneepfflplebdkc", and
// the RSA public key the following hash:
const uint8 ihfo_hash[] = {0x87, 0x5e, 0xa1, 0xa6, 0x9f, 0x85, 0xd1, 0x1e,
                           0x97, 0xd4, 0x4f, 0x55, 0xbf, 0xb4, 0x13, 0xa2,
                           0xe7, 0xc5, 0xc8, 0xf5, 0x60, 0x19, 0x78, 0x1b,
                           0x6d, 0xe9, 0x4c, 0xeb, 0x96, 0x05, 0x42, 0x17};

class TestConfigurator : public ComponentUpdateService::Configurator {
 public:
  explicit TestConfigurator();

  virtual ~TestConfigurator();

  virtual int InitialDelay() OVERRIDE;

  typedef std::pair<CrxComponent*, int> CheckAtLoopCount;

  virtual int NextCheckDelay() OVERRIDE;

  virtual int StepDelay() OVERRIDE;

  virtual int MinimumReCheckWait() OVERRIDE;

  virtual int OnDemandDelay() OVERRIDE;

  virtual GURL UpdateUrl(CrxComponent::UrlSource source) OVERRIDE;

  virtual const char* ExtraRequestParams() OVERRIDE;

  virtual size_t UrlSizeLimit() OVERRIDE;

  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE;

  // Don't use the utility process to decode files.
  virtual bool InProcess() OVERRIDE;

  virtual void OnEvent(Events event, int extra) OVERRIDE;

  virtual ComponentPatcher* CreateComponentPatcher() OVERRIDE;

  virtual bool DeltasEnabled() const OVERRIDE;

  void SetLoopCount(int times);

  void SetRecheckTime(int seconds);

  void SetOnDemandTime(int seconds);

  void AddComponentToCheck(CrxComponent* com, int at_loop_iter);

  void SetComponentUpdateService(ComponentUpdateService* cus);

 private:
  int times_;
  int recheck_time_;
  int ondemand_time_;

  std::list<CheckAtLoopCount> components_to_check_;
  ComponentUpdateService* cus_;
};

class ComponentUpdaterTest : public testing::Test {
 public:
  enum TestComponents {
    kTestComponent_abag,
    kTestComponent_jebg,
    kTestComponent_ihfo,
  };

  ComponentUpdaterTest();

  virtual ~ComponentUpdaterTest();

  virtual void TearDown();

  ComponentUpdateService* component_updater();

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  TestNotificationTracker& notification_tracker();

  TestConfigurator* test_configurator();

  ComponentUpdateService::Status RegisterComponent(CrxComponent* com,
                                                   TestComponents component,
                                                   const Version& version,
                                                   TestInstaller* installer);

 private:
  scoped_ptr<ComponentUpdateService> component_updater_;
  base::FilePath test_data_dir_;
  TestNotificationTracker notification_tracker_;
  TestConfigurator* test_config_;
};

const char expected_crx_url[] =
    "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx";

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_UPDATER_SERVICE_UNITTEST_H_
