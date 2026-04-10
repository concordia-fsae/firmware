use std::sync::OnceLock;

use anyhow::{Context, Result};
use minijinja::{context, Environment};
use serde::Serialize;

use crate::{
    ActiveFault, ControllerCapability, ControllerStatus, DashboardJob, DashboardSnapshot,
    DiagnosticSessionOption, LiveSignal, ResetOption, RoutineCapability,
};

#[derive(Debug, Clone, Serialize)]
struct FaultView {
    signal_name: String,
    has_label: bool,
    label: String,
    value: String,
    source_message: String,
    updated_at_label: String,
}

#[derive(Debug, Clone, Serialize)]
struct SignalView {
    signal_name: String,
    has_label: bool,
    label: String,
    value: String,
    source_message: String,
    updated_at_label: String,
}

#[derive(Debug, Clone, Serialize)]
struct ControllerView {
    name: String,
    slug: String,
    status_class: &'static str,
    status_label: &'static str,
    last_seen_label: String,
    fault_count: usize,
    faults: Vec<FaultView>,
    supports_critical_data: bool,
    critical_signal_count: usize,
    critical_signals: Vec<SignalView>,
    session_options: Vec<SessionOptionView>,
    reset_options: Vec<ResetOptionView>,
    routine_options: Vec<RoutineOptionView>,
    supports_operations: bool,
}

#[derive(Debug, Clone, Serialize)]
struct SessionOptionView {
    key: String,
    label: String,
}

#[derive(Debug, Clone, Serialize)]
struct RoutineOptionView {
    name: String,
    label: String,
    id_hex: String,
}

#[derive(Debug, Clone, Serialize)]
struct ResetOptionView {
    key: String,
    label: String,
}

#[derive(Debug, Clone, Serialize)]
struct JobView {
    id: String,
    operation: String,
    state: String,
    detail: String,
    payload_text: String,
    payload_hex: String,
    created_at_label: String,
    finished_at_label: String,
}

pub fn render_home(snapshot: &DashboardSnapshot, initial_state_json: String) -> Result<String> {
    let controllers = snapshot
        .controllers
        .iter()
        .map(ControllerView::from_summary_status)
        .collect::<Vec<_>>();

    environment()?
        .get_template("home.html")
        .context("loading home.html template")?
        .render(context! {
            page_title => "Dashboard",
            controllers => controllers,
            initial_state_json => initial_state_json,
        })
        .context("rendering home.html template")
}

pub fn render_signals(initial_manifest_json: &str) -> Result<String> {
    environment()?
        .get_template("signals.html")
        .context("loading signals.html template")?
        .render(context! {
            page_title => "Signal Explorer",
            initial_manifest_json => initial_manifest_json,
        })
        .context("rendering signals.html template")
}

pub fn render_controller(
    controller: &ControllerStatus,
    capability: &ControllerCapability,
    initial_controller_json: String,
    initial_jobs_json: String,
) -> Result<String> {
    let controller = ControllerView::from_status(controller, capability);
    let page_title = format!("{} Controller", controller.name);

    environment()?
        .get_template("controller.html")
        .context("loading controller.html template")?
        .render(context! {
            page_title => page_title,
            controller => controller,
            initial_controller_json => initial_controller_json,
            initial_jobs_json => initial_jobs_json,
        })
        .context("rendering controller.html template")
}

pub fn render_not_found(controller_name: &str) -> Result<String> {
    environment()?
        .get_template("error.html")
        .context("loading error.html template")?
        .render(context! {
            page_title => "Controller Not Found",
            heading => "Unknown Controller",
            message => format!(
                "The controller '{}' is not part of the deployable dashboard set.",
                controller_name
            ),
        })
        .context("rendering error.html template")
}

impl ControllerView {
    fn from_summary_status(status: &ControllerStatus) -> Self {
        Self {
            name: status.name.clone(),
            slug: status.name.clone(),
            status_class: if status.online { "online" } else { "offline" },
            status_label: if status.online { "ONLINE" } else { "OFFLINE" },
            last_seen_label: age_label(status.last_seen_ms),
            fault_count: status.faults.len(),
            faults: status.faults.iter().map(FaultView::from_fault).collect(),
            supports_critical_data: controller_supports_critical_data(&status.name),
            critical_signal_count: status.critical_signals.len(),
            critical_signals: status
                .critical_signals
                .iter()
                .map(SignalView::from_signal)
                .collect(),
            session_options: Vec::new(),
            reset_options: Vec::new(),
            routine_options: Vec::new(),
            supports_operations: false,
        }
    }

    fn from_status(status: &ControllerStatus, capability: &ControllerCapability) -> Self {
        Self {
            name: status.name.clone(),
            slug: status.name.clone(),
            status_class: if status.online { "online" } else { "offline" },
            status_label: if status.online { "ONLINE" } else { "OFFLINE" },
            last_seen_label: age_label(status.last_seen_ms),
            fault_count: status.faults.len(),
            faults: status.faults.iter().map(FaultView::from_fault).collect(),
            supports_critical_data: controller_supports_critical_data(&status.name),
            critical_signal_count: status.critical_signals.len(),
            critical_signals: status
                .critical_signals
                .iter()
                .map(SignalView::from_signal)
                .collect(),
            session_options: capability
                .sessions
                .iter()
                .map(SessionOptionView::from_option)
                .collect(),
            reset_options: capability
                .resets
                .iter()
                .map(ResetOptionView::from_option)
                .collect(),
            routine_options: capability
                .routines
                .iter()
                .map(RoutineOptionView::from_routine)
                .collect(),
            supports_operations: true,
        }
    }
}

impl SessionOptionView {
    fn from_option(option: &DiagnosticSessionOption) -> Self {
        Self {
            key: option.key.clone(),
            label: option.label.clone(),
        }
    }
}

impl RoutineOptionView {
    fn from_routine(routine: &RoutineCapability) -> Self {
        Self {
            name: routine.name.clone(),
            label: routine.label.clone(),
            id_hex: routine.id_hex.clone(),
        }
    }
}

impl ResetOptionView {
    fn from_option(option: &ResetOption) -> Self {
        Self {
            key: option.key.clone(),
            label: option.label.clone(),
        }
    }
}

impl JobView {
    #[allow(dead_code)]
    fn from_job(job: &DashboardJob) -> Self {
        Self {
            id: job.id.clone(),
            operation: job.operation.clone(),
            state: job.state.clone(),
            detail: job.detail.clone(),
            payload_text: job.payload_text.clone().unwrap_or_default(),
            payload_hex: job.payload_hex.clone().unwrap_or_default(),
            created_at_label: age_label(Some(job.created_at_ms)),
            finished_at_label: age_label(job.finished_at_ms),
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

impl SignalView {
    fn from_signal(signal: &LiveSignal) -> Self {
        Self {
            signal_name: signal.signal_name.clone(),
            has_label: signal.label.is_some(),
            label: signal.label.clone().unwrap_or_default(),
            value: signal.value.clone(),
            source_message: signal.source_message.clone(),
            updated_at_label: age_label(Some(signal.updated_at_ms)),
        }
    }
}

fn environment() -> Result<&'static Environment<'static>> {
    static ENV: OnceLock<Result<Environment<'static>, String>> = OnceLock::new();

    let cached = ENV.get_or_init(|| {
        let mut env = Environment::new();
        if let Err(error) = env
            .add_template("base.html", include_str!("../templates/base.html"))
            .context("adding base.html template")
        {
            return Err(error.to_string());
        }
        if let Err(error) = env
            .add_template("home.html", include_str!("../templates/home.html"))
            .context("adding home.html template")
        {
            return Err(error.to_string());
        }
        if let Err(error) = env
            .add_template("signals.html", include_str!("../templates/signals.html"))
            .context("adding signals.html template")
        {
            return Err(error.to_string());
        }
        if let Err(error) = env
            .add_template(
                "controller.html",
                include_str!("../templates/controller.html"),
            )
            .context("adding controller.html template")
        {
            return Err(error.to_string());
        }
        if let Err(error) = env
            .add_template("error.html", include_str!("../templates/error.html"))
            .context("adding error.html template")
        {
            return Err(error.to_string());
        }
        Ok(env)
    });

    match cached {
        Ok(env) => Ok(env),
        Err(error) => Err(anyhow::anyhow!(error.clone())),
    }
}

fn controller_supports_critical_data(controller_name: &str) -> bool {
    controller_name == "bmsb" || controller_name.starts_with("bmsw")
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
