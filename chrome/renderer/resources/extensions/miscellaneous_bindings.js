// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This contains unprivileged javascript APIs for extensions and apps.  It
// can be loaded by any extension-related context, such as content scripts or
// background pages. See user_script_slave.cc for script that is loaded by
// content scripts only.

  // TODO(kalman): factor requiring chrome out of here.
  var chrome = requireNative('chrome').GetChrome();
  var Event = require('event_bindings').Event;
  var lastError = require('lastError');
  var logActivity = requireNative('activityLogger');
  var miscNatives = requireNative('miscellaneous_bindings_natives');
  var processNatives = requireNative('process');
  var unloadEvent = require('unload_event');

  // The reserved channel name for the sendRequest/send(Native)Message APIs.
  // Note: sendRequest is deprecated.
  var kRequestChannel = "chrome.extension.sendRequest";
  var kMessageChannel = "chrome.runtime.sendMessage";
  var kNativeMessageChannel = "chrome.runtime.sendNativeMessage";

  // Map of port IDs to port object.
  var ports = {};

  // Map of port IDs to unloadEvent listeners. Keep track of these to free the
  // unloadEvent listeners when ports are closed.
  var portReleasers = {};

  // Change even to odd and vice versa, to get the other side of a given
  // channel.
  function getOppositePortId(portId) { return portId ^ 1; }

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  function Port(portId, opt_name) {
    this.portId_ = portId;
    this.name = opt_name;
    this.onDisconnect = new Event();
    this.onMessage = new Event();
  }

  // Sends a message asynchronously to the context on the other end of this
  // port.
  Port.prototype.postMessage = function(msg) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (msg === undefined)
      msg = null;
    miscNatives.PostMessage(this.portId_, $JSON.stringify(msg));
  };

  // Disconnects the port from the other end.
  Port.prototype.disconnect = function() {
    miscNatives.CloseChannel(this.portId_, true);
    this.destroy_();
  };

  Port.prototype.destroy_ = function() {
    var portId = this.portId_;

    this.onDisconnect.destroy_();
    this.onMessage.destroy_();

    miscNatives.PortRelease(portId);
    unloadEvent.removeListener(portReleasers[portId]);

    delete ports[portId];
    delete portReleasers[portId];
  };

  // Returns true if the specified port id is in this context. This is used by
  // the C++ to avoid creating the javascript message for all the contexts that
  // don't care about a particular message.
  function hasPort(portId) {
    return portId in ports;
  };

  // Hidden port creation function.  We don't want to expose an API that lets
  // people add arbitrary port IDs to the port list.
  function createPort(portId, opt_name) {
    if (ports[portId])
      throw new Error("Port '" + portId + "' already exists.");
    var port = new Port(portId, opt_name);
    ports[portId] = port;
    portReleasers[portId] = $Function.bind(miscNatives.PortRelease,
                                            this,
                                            portId);
    unloadEvent.addListener(portReleasers[portId]);
    miscNatives.PortAddRef(portId);
    return port;
  };

  // Helper function for dispatchOnRequest.
  function handleSendRequestError(isSendMessage,
                                  responseCallbackPreserved,
                                  sourceExtensionId,
                                  targetExtensionId,
                                  sourceUrl) {
    var errorMsg = [];
    var eventName = isSendMessage ? "runtime.onMessage" : "extension.onRequest";
    if (isSendMessage && !responseCallbackPreserved) {
      $Array.push(errorMsg,
          "The chrome." + eventName + " listener must return true if you " +
          "want to send a response after the listener returns");
    } else {
      $Array.push(errorMsg,
          "Cannot send a response more than once per chrome." + eventName +
          " listener per document");
    }
    $Array.push(errorMsg, "(message was sent by extension" + sourceExtensionId);
    if (sourceExtensionId != "" && sourceExtensionId != targetExtensionId)
      $Array.push(errorMsg, "for extension " + targetExtensionId);
    if (sourceUrl != "")
      $Array.push(errorMsg, "for URL " + sourceUrl);
    lastError.set(eventName, errorMsg.join(" ") + ").", null, chrome);
  }

  // Helper function for dispatchOnConnect
  function dispatchOnRequest(portId, channelName, sender,
                             sourceExtensionId, targetExtensionId, sourceUrl,
                             isExternal) {
    var isSendMessage = channelName == kMessageChannel;
    var requestEvent = null;
    if (isSendMessage) {
      if (chrome.runtime) {
        requestEvent = isExternal ? chrome.runtime.onMessageExternal
                                  : chrome.runtime.onMessage;
      }
    } else {
      if (chrome.extension) {
        requestEvent = isExternal ? chrome.extension.onRequestExternal
                                  : chrome.extension.onRequest;
      }
    }
    if (!requestEvent)
      return false;
    if (!requestEvent.hasListeners())
      return false;
    var port = createPort(portId, channelName);
    port.onMessage.addListener(function(request) {
      var responseCallbackPreserved = false;
      var responseCallback = function(response) {
        if (port) {
          port.postMessage(response);
          port.destroy_();
          port = null;
        } else {
          // We nulled out port when sending the response, and now the page
          // is trying to send another response for the same request.
          handleSendRequestError(isSendMessage, responseCallbackPreserved,
                                 sourceExtensionId, targetExtensionId);
        }
      };
      // In case the extension never invokes the responseCallback, and also
      // doesn't keep a reference to it, we need to clean up the port. Do
      // so by attaching to the garbage collection of the responseCallback
      // using some native hackery.
      miscNatives.BindToGC(responseCallback, function() {
        if (port) {
          port.destroy_();
          port = null;
        }
      });
      if (!isSendMessage) {
        requestEvent.dispatch(request, sender, responseCallback);
      } else {
        var rv = requestEvent.dispatch(request, sender, responseCallback);
        responseCallbackPreserved =
            rv && rv.results && rv.results.indexOf(true) > -1;
        if (!responseCallbackPreserved && port) {
          // If they didn't access the response callback, they're not
          // going to send a response, so clean up the port immediately.
          port.destroy_();
          port = null;
        }
      }
    });
    var eventName = (isSendMessage ?
          (isExternal ?
              "runtime.onMessageExternal" : "runtime.onMessage") :
          (isExternal ?
              "extension.onRequestExternal" : "extension.onRequest"));
    logActivity.LogEvent(targetExtensionId,
                         eventName,
                         [sourceExtensionId, sourceUrl]);
    return true;
  }

  // Called by native code when a channel has been opened to this context.
  function dispatchOnConnect(portId,
                             channelName,
                             sourceTab,
                             sourceExtensionId,
                             targetExtensionId,
                             sourceUrl) {
    // Only create a new Port if someone is actually listening for a connection.
    // In addition to being an optimization, this also fixes a bug where if 2
    // channels were opened to and from the same process, closing one would
    // close both.
    var extensionId = processNatives.GetExtensionId();
    if (targetExtensionId != extensionId)
      return false;  // not for us

    if (ports[getOppositePortId(portId)])
      return false;  // this channel was opened by us, so ignore it

    // Determine whether this is coming from another extension, so we can use
    // the right event.
    var isExternal = sourceExtensionId != extensionId;

    var sender = {};
    if (sourceExtensionId != '')
      sender.id = sourceExtensionId;
    if (sourceUrl)
      sender.url = sourceUrl;
    if (sourceTab)
      sender.tab = sourceTab;

    // Special case for sendRequest/onRequest and sendMessage/onMessage.
    if (channelName == kRequestChannel || channelName == kMessageChannel) {
      return dispatchOnRequest(portId, channelName, sender,
                               sourceExtensionId, targetExtensionId, sourceUrl,
                               isExternal);
    }

    var connectEvent = null;
    if (chrome.runtime) {
      connectEvent = isExternal ? chrome.runtime.onConnectExternal
                                : chrome.runtime.onConnect;
    }
    if (!connectEvent)
      return false;
    if (!connectEvent.hasListeners())
      return false;

    var port = createPort(portId, channelName);
    port.sender = sender;
    if (processNatives.manifestVersion < 2)
      port.tab = port.sender.tab;

    var eventName = (isExternal ?
        "runtime.onConnectExternal" : "runtime.onConnect");
    connectEvent.dispatch(port);
    logActivity.LogEvent(targetExtensionId,
                         eventName,
                         [sourceExtensionId]);
    return true;
  };

  // Called by native code when a channel has been closed.
  function dispatchOnDisconnect(portId, errorMessage) {
    var port = ports[portId];
    if (port) {
      // Update the renderer's port bookkeeping, without notifying the browser.
      miscNatives.CloseChannel(portId, false);
      if (errorMessage)
        lastError.set('Port', errorMessage, null, chrome);
      try {
        port.onDisconnect.dispatch(port);
      } finally {
        port.destroy_();
        lastError.clear(chrome);
      }
    }
  };

  // Called by native code when a message has been sent to the given port.
  function dispatchOnMessage(msg, portId) {
    var port = ports[portId];
    if (port) {
      if (msg)
        msg = $JSON.parse(msg);
      port.onMessage.dispatch(msg, port);
    }
  };

  // Shared implementation used by tabs.sendMessage and runtime.sendMessage.
  function sendMessageImpl(port, request, responseCallback) {
    if (port.name != kNativeMessageChannel)
      port.postMessage(request);

    if (port.name == kMessageChannel && !responseCallback) {
      // TODO(mpcomplete): Do this for the old sendRequest API too, after
      // verifying it doesn't break anything.
      // Go ahead and disconnect immediately if the sender is not expecting
      // a response.
      port.disconnect();
      return;
    }

    // Ensure the callback exists for the older sendRequest API.
    if (!responseCallback)
      responseCallback = function() {};

    port.onDisconnect.addListener(function() {
      // For onDisconnects, we only notify the callback if there was an error.
      try {
        if (chrome.runtime && chrome.runtime.lastError)
          responseCallback();
      } finally {
        port = null;
      }
    });
    port.onMessage.addListener(function(response) {
      try {
        responseCallback(response);
      } finally {
        port.disconnect();
        port = null;
      }
    });
  };

  function sendMessageUpdateArguments(functionName) {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   extension.sendRequest(req)     -> extension.sendRequest(null, req)
    //   extension.sendRequest(req, cb) -> extension.sendRequest(null, req, cb)
    var args = $Array.splice(arguments, 1);  // skip functionName
    var lastArg = args.length - 1;

    // responseCallback (last argument) is optional.
    var responseCallback = null;
    if (typeof(args[lastArg]) == 'function')
      responseCallback = args[lastArg--];

    // request (second argument) is required.
    var request = args[lastArg--];

    // targetId (first argument, extensionId in the manfiest) is optional.
    var targetId = null;
    if (lastArg >= 0)
      targetId = args[lastArg--];

    if (lastArg != -1)
      throw new Error('Invalid arguments to ' + functionName + '.');
    return [targetId, request, responseCallback];
  }

exports.kRequestChannel = kRequestChannel;
exports.kMessageChannel = kMessageChannel;
exports.kNativeMessageChannel = kNativeMessageChannel;
exports.Port = Port;
exports.createPort = createPort;
exports.sendMessageImpl = sendMessageImpl;
exports.sendMessageUpdateArguments = sendMessageUpdateArguments;

// For C++ code to call.
exports.hasPort = hasPort;
exports.dispatchOnConnect = dispatchOnConnect;
exports.dispatchOnDisconnect = dispatchOnDisconnect;
exports.dispatchOnMessage = dispatchOnMessage;
