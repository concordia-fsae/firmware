const MAX_CACHE_MS = 30 * 60 * 1000;

const clients = new Map();

function ensureClient(clientId) {
  if (!clients.has(clientId)) {
    clients.set(clientId, {
      ports: new Set(),
      subscriptions: new Set(),
      history: new Map(),
      eventSource: null,
      paused: false,
    });
  }
  return clients.get(clientId);
}

function trimPoints(points) {
  const cutoff = Date.now() - MAX_CACHE_MS;
  let trimCount = 0;
  while (trimCount < points.length && points[trimCount].t < cutoff) {
    trimCount += 1;
  }
  if (trimCount > 0) {
    points.splice(0, trimCount);
  }
}

function postToClient(clientId, message) {
  const client = clients.get(clientId);
  if (!client) {
    return;
  }
  client.ports.forEach((port) => {
    try {
      port.postMessage({ clientId, ...message });
    } catch (_error) {
    }
  });
}

function updateStream(clientId) {
  const client = ensureClient(clientId);
  if (client.eventSource) {
    client.eventSource.close();
    client.eventSource = null;
  }

  if (client.paused || client.subscriptions.size === 0) {
    return;
  }

  const signals = Array.from(client.subscriptions).sort().join(",");
  const source = new EventSource(`/signal-events?signals=${encodeURIComponent(signals)}`);
  source.addEventListener("signal-sample", (event) => {
    let payload = null;
    try {
      payload = JSON.parse(event.data);
    } catch (_error) {
      return;
    }
    for (const signalEvent of payload.events || []) {
      for (const sample of signalEvent.samples || []) {
        const points = client.history.get(sample.signal_id) || [];
        points.push({
          t: signalEvent.timestamp_ms,
          v: sample.value,
          label: sample.label || null,
        });
        trimPoints(points);
        client.history.set(sample.signal_id, points);
      }
    }
    postToClient(clientId, { type: "signal-batch", payload });
  });
  source.onerror = () => {
    try {
      source.close();
    } catch (_error) {
    }
    client.eventSource = null;
    setTimeout(() => {
      if (clients.has(clientId)) {
        updateStream(clientId);
      }
    }, 1000);
  };
  client.eventSource = source;
}

function snapshotForClient(clientId, signalIds) {
  const client = ensureClient(clientId);
  const selected = Array.isArray(signalIds) && signalIds.length > 0
    ? signalIds
    : Array.from(client.subscriptions);
  return {
    signals: selected.map((signalId) => ({
      signalId,
      points: (client.history.get(signalId) || []).slice(),
    })),
  };
}

onconnect = (connectEvent) => {
  const port = connectEvent.ports[0];
  let currentClientId = null;

  port.onmessage = (messageEvent) => {
    const data = messageEvent.data || {};
    if (data.type === "init") {
      currentClientId = data.clientId;
      const client = ensureClient(currentClientId);
      client.ports.add(port);
      port.postMessage({ type: "ready", clientId: currentClientId });
      return;
    }

    const clientId = data.clientId || currentClientId;
    if (!clientId) {
      return;
    }
    const client = ensureClient(clientId);

    if (data.type === "set-subscription") {
      client.subscriptions = new Set(data.signalIds || []);
      client.paused = Boolean(data.paused);
      updateStream(clientId);
      return;
    }

    if (data.type === "get-snapshot") {
      port.postMessage({
        type: "snapshot",
        clientId,
        requestId: data.requestId,
        payload: snapshotForClient(clientId, data.signalIds || []),
      });
      return;
    }

    if (data.type === "disconnect") {
      client.ports.delete(port);
      if (client.ports.size === 0 && client.eventSource) {
        client.eventSource.close();
        client.eventSource = null;
      }
    }
  };

  port.start();
};
