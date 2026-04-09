use askama::Template;

use crate::{ActiveFault, ControllerStatus, DashboardSnapshot};

#[derive(Debug, Clone)]
pub struct FaultView {
    pub signal_name: String,
    pub has_label: bool,
    pub label: String,
    pub value: String,
    pub source_message: String,
    pub updated_at_label: String,
}

#[derive(Debug, Clone)]
pub struct ControllerView {
    pub name: String,
    pub slug: String,
    pub status_class: &'static str,
    pub status_label: &'static str,
    pub last_seen_label: String,
    pub fault_count: usize,
    pub faults: Vec<FaultView>,
}

#[derive(Template)]
#[template(
    ext = "html",
    source = r#"
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>{{ page_title }}</title>
    <style>
      :root {
        --bg: #f5f1e8;
        --panel: #fffdf9;
        --ink: #1f2a2f;
        --muted: #6a7378;
        --online: #1f7a46;
        --offline: #b03a2e;
        --border: #d7d0c3;
        --accent: #274c77;
        --fault: #a63d40;
      }
      * { box-sizing: border-box; }
      body {
        margin: 0;
        font-family: Georgia, ""Times New Roman"", serif;
        background:
          radial-gradient(circle at top left, rgba(39, 76, 119, 0.12), transparent 30%),
          linear-gradient(180deg, #f8f5ee 0%, var(--bg) 100%);
        color: var(--ink);
      }
      .shell { max-width: 1100px; margin: 0 auto; padding: 24px 20px 48px; }
      .topbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 16px;
        margin-bottom: 28px;
      }
      .brand { color: var(--ink); text-decoration: none; }
      .brand-title { margin: 0; font-size: clamp(1.8rem, 4vw, 3rem); letter-spacing: -0.04em; }
      .brand-copy { margin: 6px 0 0; color: var(--muted); font-size: 0.98rem; }
      .nav { display: flex; gap: 12px; flex-wrap: wrap; }
      .nav a { color: var(--accent); text-decoration: none; font-weight: 700; }
      .page-header { margin-bottom: 24px; }
      .page-title { margin: 0 0 8px; font-size: clamp(1.8rem, 4vw, 2.8rem); letter-spacing: -0.04em; }
      .page-copy { margin: 0; color: var(--muted); }
      .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px; }
      .card {
        background: var(--panel);
        border: 1px solid var(--border);
        border-radius: 16px;
        padding: 16px 18px;
        box-shadow: 0 18px 42px rgba(31, 42, 47, 0.06);
      }
      .card-link { color: inherit; text-decoration: none; display: block; }
      .card-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
        margin-bottom: 12px;
      }
      .controller-name {
        font-size: 1.15rem;
        font-weight: 700;
        text-transform: uppercase;
        letter-spacing: 0.03em;
      }
      .badge {
        border-radius: 999px;
        padding: 4px 10px;
        font-size: 0.8rem;
        font-weight: 700;
        color: white;
      }
      .badge.online { background: var(--online); }
      .badge.offline { background: var(--offline); }
      .status-line { display: flex; justify-content: space-between; gap: 12px; margin-bottom: 12px; }
      .meta { color: var(--muted); font-size: 0.92rem; }
      details { border-top: 1px solid var(--border); padding-top: 10px; }
      summary { cursor: pointer; color: var(--accent); font-weight: 700; }
      .fault-list { margin: 10px 0 0; padding-left: 18px; }
      .fault-list li { margin: 7px 0; }
      .fault-label { color: var(--fault); font-weight: 700; }
      .fault-meta { color: var(--muted); font-size: 0.88rem; }
      .empty { color: var(--muted); margin: 10px 0 0; }
      @media (max-width: 640px) {
        .shell { padding: 20px 14px 40px; }
        .topbar { align-items: flex-start; flex-direction: column; }
      }
    </style>
  </head>
  <body>
    <main class="shell">
      <header class="topbar">
        <a href="/" class="brand">
          <h1 class="brand-title">Controller Dashboard</h1>
          <p class="brand-copy">Live status, faults, and room to grow into deeper pages.</p>
        </a>
        <nav class="nav">
          <a href="/">Home</a>
        </nav>
      </header>
      <section class="page-header">
        <h2 class="page-title">Deployable Controllers</h2>
        <p class="page-copy">Controllers update live over SSE. Select a controller page as deeper views are added.</p>
      </section>
      <section id="controllers" class="grid">
        {% for controller in controllers %}
          <article class="card">
            <a class="card-link" href="/controllers/{{ controller.slug }}">
              <div class="card-header">
                <div class="controller-name">{{ controller.name }}</div>
                <span class="badge {{ controller.status_class }}">{{ controller.status_label }}</span>
              </div>
              <div class="status-line">
                <span class="meta">Last seen: {{ controller.last_seen_label }}</span>
                <span class="meta">Faults: {{ controller.fault_count }}</span>
              </div>
            </a>
            <details>
              <summary>Fault Details</summary>
              {% if controller.fault_count == 0 %}
                <p class="empty">No active faults.</p>
              {% else %}
                <ul class="fault-list">
                  {% for fault in controller.faults %}
                    <li>
                      <span class="fault-label">{{ fault.signal_name }}</span>
                      {% if fault.has_label %}
                        <span>{{ fault.label }}</span>
                      {% endif %}
                      <div class="fault-meta">
                        value={{ fault.value }} · source={{ fault.source_message }} · updated {{ fault.updated_at_label }}
                      </div>
                    </li>
                  {% endfor %}
                </ul>
              {% endif %}
            </details>
          </article>
        {% endfor %}
      </section>
      <script id="initial-state" type="application/json">{{ initial_state_json|safe }}</script>
    </main>
    <script>
      const initialState = JSON.parse(document.getElementById("initial-state").textContent);
      const container = document.getElementById("controllers");
      let latestState = initialState;

      function escapeHtml(value) {
        return String(value)
          .replaceAll("&", "&amp;")
          .replaceAll("<", "&lt;")
          .replaceAll(">", "&gt;")
          .replaceAll('"', "&quot;")
          .replaceAll("'", "&#39;");
      }

      function ageLabel(timestampMs) {
        if (!timestampMs) return "Never";
        const diffMs = Math.max(0, Date.now() - timestampMs);
        if (diffMs < 1000) return "just now";
        const secs = Math.floor(diffMs / 1000);
        if (secs < 60) return `${secs}s ago`;
        const mins = Math.floor(secs / 60);
        if (mins < 60) return `${mins}m ago`;
        const hours = Math.floor(mins / 60);
        return `${hours}h ago`;
      }

      function renderFaults(controller) {
        const faults = controller.faults || [];
        if (!faults.length) {
          return `<p class="empty">No active faults.</p>`;
        }
        return `<ul class="fault-list">${faults.map((fault) => `
          <li>
            <span class="fault-label">${escapeHtml(fault.signal_name)}</span>
            ${fault.label ? `<span>${escapeHtml(fault.label)}</span>` : ""}
            <div class="fault-meta">
              value=${escapeHtml(fault.value)} · source=${escapeHtml(fault.source_message)} · updated ${ageLabel(fault.updated_at_ms)}
            </div>
          </li>
        `).join("")}</ul>`;
      }

      function render(state) {
        latestState = state;
        container.innerHTML = state.controllers.map((controller) => {
          const online = Boolean(controller.online);
          const statusClass = online ? "online" : "offline";
          const statusLabel = online ? "ONLINE" : "OFFLINE";
          const faultCount = (controller.faults || []).length;
          return `
            <article class="card">
              <a class="card-link" href="/controllers/${encodeURIComponent(controller.name)}">
                <div class="card-header">
                  <div class="controller-name">${escapeHtml(controller.name)}</div>
                  <span class="badge ${statusClass}">${statusLabel}</span>
                </div>
                <div class="status-line">
                  <span class="meta">Last seen: ${ageLabel(controller.last_seen_ms)}</span>
                  <span class="meta">Faults: ${faultCount}</span>
                </div>
              </a>
              <details>
                <summary>Fault Details</summary>
                ${renderFaults(controller)}
              </details>
            </article>
          `;
        }).join("");
      }

      render(initialState);
      setInterval(() => render(latestState), 1000);

      const source = new EventSource("/events");
      source.addEventListener("state", (event) => {
        render(JSON.parse(event.data));
      });
    </script>
  </body>
</html>
"#
)]
pub struct HomeTemplate {
    pub page_title: &'static str,
    pub controllers: Vec<ControllerView>,
    pub initial_state_json: String,
}

#[derive(Template)]
#[template(
    ext = "html",
    source = r#"
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>{{ page_title }}</title>
    <style>
      :root {
        --bg: #f5f1e8;
        --panel: #fffdf9;
        --ink: #1f2a2f;
        --muted: #6a7378;
        --online: #1f7a46;
        --offline: #b03a2e;
        --border: #d7d0c3;
        --accent: #274c77;
        --fault: #a63d40;
      }
      * { box-sizing: border-box; }
      body {
        margin: 0;
        font-family: Georgia, ""Times New Roman"", serif;
        background:
          radial-gradient(circle at top left, rgba(39, 76, 119, 0.12), transparent 30%),
          linear-gradient(180deg, #f8f5ee 0%, var(--bg) 100%);
        color: var(--ink);
      }
      .shell { max-width: 1100px; margin: 0 auto; padding: 24px 20px 48px; }
      .topbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 16px;
        margin-bottom: 28px;
      }
      .brand { color: var(--ink); text-decoration: none; }
      .brand-title { margin: 0; font-size: clamp(1.8rem, 4vw, 3rem); letter-spacing: -0.04em; }
      .brand-copy { margin: 6px 0 0; color: var(--muted); font-size: 0.98rem; }
      .nav { display: flex; gap: 12px; flex-wrap: wrap; }
      .nav a, .back-link { color: var(--accent); text-decoration: none; font-weight: 700; }
      .page-header { margin-bottom: 24px; }
      .page-title { margin: 0 0 8px; font-size: clamp(1.8rem, 4vw, 2.8rem); letter-spacing: -0.04em; }
      .page-copy { margin: 0; color: var(--muted); }
      .card {
        background: var(--panel);
        border: 1px solid var(--border);
        border-radius: 16px;
        padding: 16px 18px;
        box-shadow: 0 18px 42px rgba(31, 42, 47, 0.06);
      }
      .card-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
        margin-bottom: 12px;
      }
      .controller-name {
        font-size: 1.15rem;
        font-weight: 700;
        text-transform: uppercase;
        letter-spacing: 0.03em;
      }
      .badge {
        border-radius: 999px;
        padding: 4px 10px;
        font-size: 0.8rem;
        font-weight: 700;
        color: white;
      }
      .badge.online { background: var(--online); }
      .badge.offline { background: var(--offline); }
      .status-line { display: flex; justify-content: space-between; gap: 12px; margin-bottom: 12px; }
      .meta { color: var(--muted); font-size: 0.92rem; }
      details { border-top: 1px solid var(--border); padding-top: 10px; }
      summary { cursor: pointer; color: var(--accent); font-weight: 700; }
      .fault-list { margin: 10px 0 0; padding-left: 18px; }
      .fault-list li { margin: 7px 0; }
      .fault-label { color: var(--fault); font-weight: 700; }
      .fault-meta { color: var(--muted); font-size: 0.88rem; }
      .empty { color: var(--muted); margin: 10px 0 0; }
      @media (max-width: 640px) {
        .shell { padding: 20px 14px 40px; }
        .topbar { align-items: flex-start; flex-direction: column; }
      }
    </style>
  </head>
  <body>
    <main class="shell">
      <header class="topbar">
        <a href="/" class="brand">
          <h1 class="brand-title">Controller Dashboard</h1>
          <p class="brand-copy">Live status, faults, and room to grow into deeper pages.</p>
        </a>
        <nav class="nav">
          <a href="/">Home</a>
        </nav>
      </header>
      <a class="back-link" href="/">Back to all controllers</a>
      <section class="page-header">
        <h2 class="page-title">{{ controller.name }}</h2>
        <p class="page-copy">This page tracks one controller and can grow into signals and graphing later.</p>
      </section>
      <section id="controller-card">
        <article class="card">
          <div class="card-header">
            <div class="controller-name">{{ controller.name }}</div>
            <span class="badge {{ controller.status_class }}">{{ controller.status_label }}</span>
          </div>
          <div class="status-line">
            <span class="meta">Last seen: {{ controller.last_seen_label }}</span>
            <span class="meta">Faults: {{ controller.fault_count }}</span>
          </div>
          <details open>
            <summary>Fault Details</summary>
            {% if controller.fault_count == 0 %}
              <p class="empty">No active faults.</p>
            {% else %}
              <ul class="fault-list">
                {% for fault in controller.faults %}
                  <li>
                    <span class="fault-label">{{ fault.signal_name }}</span>
                    {% if fault.has_label %}
                      <span>{{ fault.label }}</span>
                    {% endif %}
                    <div class="fault-meta">
                      value={{ fault.value }} · source={{ fault.source_message }} · updated {{ fault.updated_at_label }}
                    </div>
                  </li>
                {% endfor %}
              </ul>
            {% endif %}
          </details>
        </article>
      </section>
      <script id="initial-controller" type="application/json">{{ initial_controller_json|safe }}</script>
    </main>
    <script>
      const initialController = JSON.parse(document.getElementById("initial-controller").textContent);
      const controllerCard = document.getElementById("controller-card");
      const controllerName = initialController.name;
      let latestController = initialController;

      function escapeHtml(value) {
        return String(value)
          .replaceAll("&", "&amp;")
          .replaceAll("<", "&lt;")
          .replaceAll(">", "&gt;")
          .replaceAll('"', "&quot;")
          .replaceAll("'", "&#39;");
      }

      function ageLabel(timestampMs) {
        if (!timestampMs) return "Never";
        const diffMs = Math.max(0, Date.now() - timestampMs);
        if (diffMs < 1000) return "just now";
        const secs = Math.floor(diffMs / 1000);
        if (secs < 60) return `${secs}s ago`;
        const mins = Math.floor(secs / 60);
        if (mins < 60) return `${mins}m ago`;
        const hours = Math.floor(mins / 60);
        return `${hours}h ago`;
      }

      function renderFaults(controller) {
        const faults = controller.faults || [];
        if (!faults.length) {
          return `<p class="empty">No active faults.</p>`;
        }
        return `<ul class="fault-list">${faults.map((fault) => `
          <li>
            <span class="fault-label">${escapeHtml(fault.signal_name)}</span>
            ${fault.label ? `<span>${escapeHtml(fault.label)}</span>` : ""}
            <div class="fault-meta">
              value=${escapeHtml(fault.value)} · source=${escapeHtml(fault.source_message)} · updated ${ageLabel(fault.updated_at_ms)}
            </div>
          </li>
        `).join("")}</ul>`;
      }

      function render(controller) {
        latestController = controller;
        const online = Boolean(controller.online);
        const statusClass = online ? "online" : "offline";
        const statusLabel = online ? "ONLINE" : "OFFLINE";
        const faultCount = (controller.faults || []).length;
        controllerCard.innerHTML = `
          <article class="card">
            <div class="card-header">
              <div class="controller-name">${escapeHtml(controller.name)}</div>
              <span class="badge ${statusClass}">${statusLabel}</span>
            </div>
            <div class="status-line">
              <span class="meta">Last seen: ${ageLabel(controller.last_seen_ms)}</span>
              <span class="meta">Faults: ${faultCount}</span>
            </div>
            <details open>
              <summary>Fault Details</summary>
              ${renderFaults(controller)}
            </details>
          </article>
        `;
      }

      render(initialController);
      setInterval(() => render(latestController), 1000);

      const source = new EventSource("/events");
      source.addEventListener("state", (event) => {
        const snapshot = JSON.parse(event.data);
        const controller = (snapshot.controllers || []).find((entry) => entry.name === controllerName);
        if (controller) {
          render(controller);
        }
      });
    </script>
  </body>
</html>
"#
)]
pub struct ControllerTemplate {
    pub page_title: String,
    pub controller: ControllerView,
    pub initial_controller_json: String,
}

#[derive(Template)]
#[template(
    ext = "html",
    source = r#"
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>{{ page_title }}</title>
    <style>
      :root {
        --bg: #f5f1e8;
        --panel: #fffdf9;
        --ink: #1f2a2f;
        --muted: #6a7378;
        --accent: #274c77;
      }
      body {
        margin: 0;
        font-family: Georgia, ""Times New Roman"", serif;
        background:
          radial-gradient(circle at top left, rgba(39, 76, 119, 0.12), transparent 30%),
          linear-gradient(180deg, #f8f5ee 0%, var(--bg) 100%);
        color: var(--ink);
      }
      .shell { max-width: 1100px; margin: 0 auto; padding: 24px 20px 48px; }
      .brand { color: var(--ink); text-decoration: none; }
      .brand-title { margin: 0; font-size: clamp(1.8rem, 4vw, 3rem); letter-spacing: -0.04em; }
      .brand-copy { margin: 6px 0 0; color: var(--muted); font-size: 0.98rem; }
      .page-header { margin: 28px 0 24px; }
      .page-title { margin: 0 0 8px; font-size: clamp(1.8rem, 4vw, 2.8rem); letter-spacing: -0.04em; }
      .page-copy, .back-link { color: var(--accent); text-decoration: none; }
    </style>
  </head>
  <body>
    <main class="shell">
      <a href="/" class="brand">
        <h1 class="brand-title">Controller Dashboard</h1>
        <p class="brand-copy">Live status, faults, and room to grow into deeper pages.</p>
      </a>
      <section class="page-header">
        <h2 class="page-title">{{ heading }}</h2>
        <p class="page-copy">{{ message }}</p>
      </section>
      <a class="back-link" href="/">Return to the dashboard</a>
    </main>
  </body>
</html>
"#
)]
pub struct ErrorTemplate {
    pub page_title: String,
    pub heading: String,
    pub message: String,
}

impl HomeTemplate {
    pub fn new(snapshot: &DashboardSnapshot, initial_state_json: String) -> Self {
        Self {
            page_title: "Dashboard",
            controllers: snapshot
                .controllers
                .iter()
                .map(ControllerView::from_status)
                .collect(),
            initial_state_json,
        }
    }
}

impl ControllerTemplate {
    pub fn new(controller: &ControllerStatus, initial_controller_json: String) -> Self {
        Self {
            page_title: format!("{} Controller", controller.name),
            controller: ControllerView::from_status(controller),
            initial_controller_json,
        }
    }
}

impl ErrorTemplate {
    pub fn not_found(controller_name: &str) -> Self {
        Self {
            page_title: "Controller Not Found".to_string(),
            heading: "Unknown Controller".to_string(),
            message: format!(
                "The controller '{}' is not part of the deployable dashboard set.",
                controller_name
            ),
        }
    }
}

impl ControllerView {
    fn from_status(status: &ControllerStatus) -> Self {
        Self {
            name: status.name.clone(),
            slug: status.name.clone(),
            status_class: if status.online { "online" } else { "offline" },
            status_label: if status.online { "ONLINE" } else { "OFFLINE" },
            last_seen_label: age_label(status.last_seen_ms),
            fault_count: status.faults.len(),
            faults: status.faults.iter().map(FaultView::from_fault).collect(),
        }
    }
}

impl FaultView {
    fn from_fault(fault: &ActiveFault) -> Self {
        Self {
            signal_name: fault.signal_name.clone(),
            has_label: fault.label.is_some(),
            label: fault.label.clone().unwrap_or_default(),
            value: fault.value.clone(),
            source_message: fault.source_message.clone(),
            updated_at_label: age_label(Some(fault.updated_at_ms)),
        }
    }
}

fn age_label(timestamp_ms: Option<u64>) -> String {
    let Some(timestamp_ms) = timestamp_ms else {
        return "Never".to_string();
    };

    let now_ms = js_safe_now_ms();
    let diff_ms = now_ms.saturating_sub(timestamp_ms);
    if diff_ms < 1_000 {
        return "just now".to_string();
    }

    let secs = diff_ms / 1_000;
    if secs < 60 {
        return format!("{secs}s ago");
    }

    let mins = secs / 60;
    if mins < 60 {
        return format!("{mins}m ago");
    }

    let hours = mins / 60;
    format!("{hours}h ago")
}

fn js_safe_now_ms() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};

    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}
