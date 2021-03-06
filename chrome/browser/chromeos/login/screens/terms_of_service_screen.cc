// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"

#include <string>

#include "base/location.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace chromeos {

TermsOfServiceScreen::TermsOfServiceScreen(ScreenObserver* screen_observer,
                                           TermsOfServiceScreenActor* actor)
    : WizardScreen(screen_observer),
      actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

TermsOfServiceScreen::~TermsOfServiceScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void TermsOfServiceScreen::PrepareToShow() {
}

void TermsOfServiceScreen::Show() {
  if (!actor_)
    return;

  // Set the domain name whose Terms of Service are being shown.
  actor_->SetDomain(
      g_browser_process->browser_policy_connector()->GetEnterpriseDomain());

  // Show the screen.
  actor_->Show();

  // Start downloading the Terms of Service.
  StartDownload();
}

void TermsOfServiceScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string TermsOfServiceScreen::GetName() const {
  return WizardController::kTermsOfServiceScreenName;
}

void TermsOfServiceScreen::OnDecline() {
  get_screen_observer()->OnExit(ScreenObserver::TERMS_OF_SERVICE_DECLINED);
}

void TermsOfServiceScreen::OnAccept() {
  get_screen_observer()->OnExit(ScreenObserver::TERMS_OF_SERVICE_ACCEPTED);
}

void TermsOfServiceScreen::OnActorDestroyed(TermsOfServiceScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void TermsOfServiceScreen::StartDownload() {
  const PrefService* prefs = ProfileManager::GetDefaultProfile()->GetPrefs();
  // If an URL from which the Terms of Service can be downloaded has not been
  // set, show an error message to the user.
  std::string terms_of_service_url =
      prefs->GetString(prefs::kTermsOfServiceURL);
  if (terms_of_service_url.empty()) {
    if (actor_)
      actor_->OnLoadError();
    return;
  }

  // Start downloading the Terms of Service.
  terms_of_service_fetcher_.reset(net::URLFetcher::Create(
      GURL(terms_of_service_url), net::URLFetcher::GET, this));
  terms_of_service_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  // Request a text/plain MIME type as only plain-text Terms of Service are
  // accepted.
  terms_of_service_fetcher_->AddExtraRequestHeader("Accept: text/plain");
  // Retry up to three times if network changes are detected during the
  // download.
  terms_of_service_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  terms_of_service_fetcher_->Start();

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(1),
                        this, &TermsOfServiceScreen::OnDownloadTimeout);
}

void TermsOfServiceScreen::OnDownloadTimeout() {
  // Abort the download attempt.
  terms_of_service_fetcher_.reset();

  // Show an error message to the user.
  if (actor_)
    actor_->OnLoadError();
}

void TermsOfServiceScreen::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source != terms_of_service_fetcher_.get()) {
    NOTREACHED() << "Callback from foreign URL fetcher";
    return;
  }

  download_timer_.Stop();

  // Destroy the fetcher when this method returns.
  scoped_ptr<net::URLFetcher> fetcher(terms_of_service_fetcher_.Pass());
  if (!actor_)
    return;

  std::string terms_of_service;
  std::string mime_type;
  // If the Terms of Service could not be downloaded, do not have a MIME type of
  // text/plain or are empty, show an error message to the user.
  if (!source->GetStatus().is_success() ||
      !source->GetResponseHeaders()->GetMimeType(&mime_type) ||
      mime_type != "text/plain" ||
      !source->GetResponseAsString(&terms_of_service)) {
    actor_->OnLoadError();
  } else {
    // If the Terms of Service were downloaded successfully, show them to the
    // user.
    actor_->OnLoadSuccess(terms_of_service);
  }
}

}  // namespace chromeos
