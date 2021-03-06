# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cron_servlet import CronServlet
from patch_servlet import PatchServlet
from instance_servlet import InstanceServlet
from servlet import Servlet, Request, Response

_DEFAULT_SERVLET = InstanceServlet.GetConstructor()
_SERVLETS = {
  'cron': CronServlet,
  'patch': PatchServlet,
}

class Handler(Servlet):
  def Get(self):
    path = self._request.path

    if path.startswith('_'):
      servlet_path = path[1:]
      if not '/' in servlet_path:
        servlet_path += '/'
      servlet_name, servlet_path = servlet_path.split('/', 1)
      servlet = _SERVLETS.get(servlet_name)
      if servlet is None:
        return Response.NotFound('"%s" servlet not found' %  servlet_path)
    else:
      servlet_path = path
      servlet = _DEFAULT_SERVLET

    return servlet(
      Request(servlet_path, self._request.host, self._request.headers)).Get()
