use super::cluster::{ClusterNodeResetFn, ClusterNodeRunForFn, ClusterPythonScheduledFn};
use super::scheduler::SchedulerCallbackContext;

#[derive(Clone, Copy)]
pub(super) enum ClusterNodeScheduler {
    RustRuntimeModel,
    External {
        run_for: ClusterNodeRunForFn,
    },
    Python {
        scheduled: Option<ClusterPythonScheduledFn>,
        period_ns: u64,
        input_pending: bool,
    },
}

#[derive(Clone, Copy)]
pub(super) struct ClusterNode {
    pub(super) scheduler: ClusterNodeScheduler,
    pub(super) reset: Option<ClusterNodeResetFn>,
    pub(super) online: bool,
    pub(super) elapsed_ns: u64,
}

impl ClusterNode {
    pub(super) fn external(
        run_for: ClusterNodeRunForFn,
        reset: ClusterNodeResetFn,
        online: bool,
    ) -> Self {
        Self {
            scheduler: ClusterNodeScheduler::External { run_for },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        }
    }

    pub(super) fn rust_runtime_model(online: bool) -> Self {
        Self {
            scheduler: ClusterNodeScheduler::RustRuntimeModel,
            reset: None,
            online,
            elapsed_ns: 0,
        }
    }

    pub(super) fn python(
        scheduled: Option<ClusterPythonScheduledFn>,
        reset: ClusterNodeResetFn,
        period_ns: u64,
        online: bool,
    ) -> Self {
        Self {
            scheduler: ClusterNodeScheduler::Python {
                scheduled,
                period_ns,
                input_pending: false,
            },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        }
    }

    pub(super) fn needs_run_step(&self) -> bool {
        matches!(self.scheduler, ClusterNodeScheduler::External { .. })
    }

    pub(super) fn python_period_ns(&self) -> Option<u64> {
        match self.scheduler {
            ClusterNodeScheduler::Python {
                scheduled: Some(_),
                period_ns,
                ..
            } if period_ns != 0 => Some(period_ns),
            _ => None,
        }
    }

    pub(super) fn has_python_input_callback(&self) -> bool {
        matches!(
            self.scheduler,
            ClusterNodeScheduler::Python {
                scheduled: Some(_),
                ..
            }
        )
    }

    pub(super) fn run_for(&mut self, delta_ns: u64) {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => {}
            ClusterNodeScheduler::External { run_for, .. } => {
                unsafe { run_for(delta_ns) };
            }
            ClusterNodeScheduler::Python { .. } => {}
        }
        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
    }

    pub(super) fn mark_input_pending(&mut self) {
        if let ClusterNodeScheduler::Python { input_pending, .. } = &mut self.scheduler {
            *input_pending = true;
        }
    }

    pub(super) fn clear_input_pending(&mut self) {
        if let ClusterNodeScheduler::Python { input_pending, .. } = &mut self.scheduler {
            *input_pending = false;
        }
    }

    pub(super) fn run_python_algorithm(&mut self, cluster_elapsed_ns: u64) {
        let ClusterNodeScheduler::Python {
            scheduled,
            input_pending,
            ..
        } = &mut self.scheduler
        else {
            return;
        };

        if let Some(scheduled) = scheduled {
            let context = SchedulerCallbackContext {
                elapsed_ns: cluster_elapsed_ns,
                delta_ns: cluster_elapsed_ns.saturating_sub(self.elapsed_ns),
            };
            unsafe {
                scheduled(&context);
            };
        }
        self.elapsed_ns = cluster_elapsed_ns;
        *input_pending = false;
    }

    pub(super) fn python_input_pending(&self) -> bool {
        matches!(
            self.scheduler,
            ClusterNodeScheduler::Python {
                input_pending: true,
                ..
            }
        )
    }
}
