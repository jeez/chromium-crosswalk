// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some basic types used by the P2P-related IPC
// messages.

#ifndef CONTENT_COMMON_P2P_SOCKETS_H_
#define CONTENT_COMMON_P2P_SOCKETS_H_

namespace content {

// Type of P2P Socket.
enum P2PSocketType {
  P2P_SOCKET_UDP,
  P2P_SOCKET_TCP_SERVER,
  P2P_SOCKET_STUN_TCP_SERVER,
  P2P_SOCKET_TCP_CLIENT,
  P2P_SOCKET_STUN_TCP_CLIENT,
  P2P_SOCKET_SSLTCP_CLIENT,
  P2P_SOCKET_STUN_SSLTCP_CLIENT,
};

}  // namespace content

#endif  // CONTENT_COMMON_P2P_SOCKETS_H_
