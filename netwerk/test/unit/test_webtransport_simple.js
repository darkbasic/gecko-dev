//
//  Simple WebTransport test
//

"use strict";

var h3Port;
var host;

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let WebTransportListener = function() {};

WebTransportListener.prototype = {
  onSessionReady(sessionId) {
    info("SessionId " + sessionId);
    this.ready();
  },
  onSessionClosed(errorCode, reason) {
    info("Error: " + errorCode + " reason: " + reason);
    this.closed();
  },

  QueryInterface: ChromeUtils.generateQI(["WebTransportSessionEventListener"]),
};

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.localDomains");
});

add_task(async function setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");

  h3Port = env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  host = "foo.example.com:" + h3Port;
  do_get_profile();

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // `../unit/` so that unit_ipc tests can use as well
  addCertFromFile(certdb, "../unit/http2-ca.pem", "CTu,u,u");
});

add_task(async function test_connect_wt() {
  let webTransport = NetUtil.newWebTransport();

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI("https://" + host + "/success"),
      Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  webTransport.closeSession(0, "");
});

add_task(async function test_reject() {
  let webTransport = NetUtil.newWebTransport();

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.closed = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI("https://" + host + "/reject"),
      Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });
});

async function test_closed(path) {
  let webTransport = NetUtil.newWebTransport();

  let listener = new WebTransportListener().QueryInterface(
    Ci.WebTransportSessionEventListener
  );

  let pReady = new Promise(resolve => {
    listener.ready = resolve;
  });
  let pClose = new Promise(resolve => {
    listener.closed = resolve;
  });
  webTransport.asyncConnect(
    NetUtil.newURI("https://" + host + path),
    Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal),
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    listener
  );

  await pReady;
  await pClose;
}

add_task(async function test_closed_0ms() {
  test_closed("/closeafter0ms");
});

add_task(async function test_closed_100ms() {
  test_closed("/closeafter100ms");
});

function WebTransportStreamCallback() {}

WebTransportStreamCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIWebTransportStreamCallback"]),

  onBidirectionalStreamReady(aStream) {
    Assert.ok(aStream != null);
    this.finish(aStream);
  },
  onUnidirectionalStreamReady(aStream) {
    Assert.ok(aStream != null);
    this.finish(aStream);
  },
  onError(aError) {
    this.finish(aError);
  },
};

function streamCreatePromise(transport, bidi) {
  return new Promise(resolve => {
    let listener = new WebTransportStreamCallback().QueryInterface(
      Ci.nsIWebTransportStreamCallback
    );
    listener.finish = resolve;

    if (bidi) {
      transport.createOutgoingBidirectionalStream(listener);
    } else {
      transport.createOutgoingUnidirectionalStream(listener);
    }
  });
}

add_task(async function test_wt_stream_create() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  let error = await streamCreatePromise(webTransport, true);
  Assert.equal(error, Ci.nsIWebTransport.INVALID_STATE_ERROR);

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI("https://" + host + "/success"),
      Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  await streamCreatePromise(webTransport, true);
  await streamCreatePromise(webTransport, false);

  webTransport.closeSession(0, "");
});

function SendStreamStatsCallback() {}

SendStreamStatsCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebTransportSendStreamStatsCallback",
  ]),

  onStatsAvailable(aStats) {
    Assert.ok(aStats != null);
    this.finish(aStats);
  },
};

function sendStreamStatsPromise(stream) {
  return new Promise(resolve => {
    let listener = new SendStreamStatsCallback().QueryInterface(
      Ci.nsIWebTransportSendStreamStatsCallback
    );
    listener.finish = resolve;

    stream.QueryInterface(Ci.nsIWebTransportSendStream);
    stream.getSendStreamStats(listener);
  });
}

add_task(async function test_wt_stream_send_and_stats() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI("https://" + host + "/success"),
      Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  let stream = await streamCreatePromise(webTransport, false);
  stream.QueryInterface(Ci.nsIAsyncOutputStream);

  let data = "123456";
  stream.write(data, data.length);

  // We need some time to send the packet out.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  let stats = await sendStreamStatsPromise(stream);
  Assert.equal(stats.bytesWritten, data.length);
  Assert.equal(stats.bytesSent, data.length);

  webTransport.closeSession(0, "");
});
