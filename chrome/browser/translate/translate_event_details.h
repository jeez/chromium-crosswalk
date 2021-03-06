// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_EVENT_DETAILS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_EVENT_DETAILS_H_

#include <string>

#include "base/time.h"

struct TranslateEventDetails {
  TranslateEventDetails(const std::string& in_filename,
                        int in_line,
                        const std::string& in_message);

  // The time when this event was created.
  base::Time time;

  // The source filename where this event was created.
  std::string filename;

  // The source line in |filename| where this event was created.
  int line;

  // The message to show in event logs.
  std::string message;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_EVENT_DETAILS_H_
