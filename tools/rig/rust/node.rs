use super::scheduler::SchedulerCallbackContext;

pub type RigNodeRunForFn = unsafe extern "C" fn(u64);
pub type RigNodeResetFn = unsafe extern "C" fn();
pub type RigPythonScheduledFn = unsafe extern "C" fn(*const SchedulerCallbackContext);

#[derive(Clone, Copy)]
pub enum RigNodeScheduler {
    RustRuntimeModel,
    External {
        run_for: RigNodeRunForFn,
    },
    Python {
        scheduled: Option<RigPythonScheduledFn>,
        period_ns: u64,
        input_pending: bool,
    },
}

#[derive(Clone, Copy)]
pub struct RigNode {
    pub(super) scheduler: RigNodeScheduler,
    pub(super) reset: Option<RigNodeResetFn>,
    pub(super) online: bool,
    pub(super) elapsed_ns: u64,
}

impl RigNode {
    pub(super) fn external(run_for: RigNodeRunForFn, reset: RigNodeResetFn, online: bool) -> Self {
        Self {
            scheduler: RigNodeScheduler::External { run_for },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        }
    }

    pub(super) fn rust_runtime_model(online: bool) -> Self {
        Self {
            scheduler: RigNodeScheduler::RustRuntimeModel,
            reset: None,
            online,
            elapsed_ns: 0,
        }
    }

    pub(super) fn python(
        scheduled: Option<RigPythonScheduledFn>,
        reset: RigNodeResetFn,
        period_ns: u64,
        online: bool,
    ) -> Self {
        Self {
            scheduler: RigNodeScheduler::Python {
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
        matches!(self.scheduler, RigNodeScheduler::External { .. })
    }

    pub(super) fn python_period_ns(&self) -> Option<u64> {
        match self.scheduler {
            RigNodeScheduler::Python {
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
            RigNodeScheduler::Python {
                scheduled: Some(_),
                ..
            }
        )
    }

    pub(super) fn run_for(&mut self, delta_ns: u64) {
        match self.scheduler {
            RigNodeScheduler::RustRuntimeModel => {}
            RigNodeScheduler::External { run_for, .. } => {
                unsafe { run_for(delta_ns) };
            }
            RigNodeScheduler::Python { .. } => {}
        }
        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
    }

    pub(super) fn mark_input_pending(&mut self) {
        if let RigNodeScheduler::Python { input_pending, .. } = &mut self.scheduler {
            *input_pending = true;
        }
    }

    pub(super) fn run_python_algorithm(&mut self, cluster_elapsed_ns: u64) {
        let RigNodeScheduler::Python {
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
            RigNodeScheduler::Python {
                input_pending: true,
                ..
            }
        )
    }
}
