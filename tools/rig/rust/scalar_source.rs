// Generic periodic scalar sources owned by Rig.
//
// A scalar source is a dataflow producer. It has no knowledge of a peripheral,
// firmware backend, or model; consumers provide only the node, route, period,
// and value reader.

use std::collections::VecDeque;
use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowRuntime};
use super::scalar::{self, ScalarEvent};

pub type ScalarSourceReader = fn() -> f32;

/// A mutable, Rust-owned scalar producer for a group of related routes.
///
/// The bank is deliberately independent of any peripheral or model. Clients
/// configure route values through the host ABI, while Rig owns event storage,
/// timestamps, and routing into the dataflow graph. A host may publish one
/// complete sample for the bank at each model timestep.
#[derive(Clone, Copy)]
struct ScalarSourceChannelRef {
    bank_index: usize,
    channel_index: usize,
}

struct ScalarSourceChannel {
    route_id: u32,
    value: f32,
    pending: VecDeque<ScalarEvent>,
}

struct ScalarSourceBank {
    node: u32,
    period_ns: u64,
    channels: Vec<ScalarSourceChannel>,
}

#[derive(Default)]
struct ScalarSourceBanks {
    banks: Vec<ScalarSourceBank>,
    channel_refs: Vec<ScalarSourceChannelRef>,
}

impl ScalarSourceBanks {
    fn reset(&mut self) {
        self.banks.clear();
        self.channel_refs.clear();
    }
}

static SCALAR_SOURCE_BANKS: LazyLock<Mutex<ScalarSourceBanks>> =
    LazyLock::new(|| Mutex::new(ScalarSourceBanks::default()));

#[derive(Clone, Copy)]
struct PeriodicScalarSource {
    node: u32,
    route_id: u32,
    period_ns: u64,
    last_emit_ns: u64,
    reader: ScalarSourceReader,
}

impl PeriodicScalarSource {
    fn due_at_ns(&self) -> u64 {
        self.last_emit_ns.saturating_add(self.period_ns)
    }

    fn has_pending_event(&self, elapsed_ns: u64) -> bool {
        elapsed_ns.saturating_sub(self.last_emit_ns) >= self.period_ns
    }

    fn emit_if_due(&mut self, elapsed_ns: u64) -> Option<ScalarEvent> {
        if !self.has_pending_event(elapsed_ns) {
            return None;
        }
        self.last_emit_ns = elapsed_ns;
        Some(ScalarEvent {
            value: (self.reader)(),
            timestamp_ns: elapsed_ns,
        })
    }
}

#[derive(Default)]
struct PeriodicScalarSources {
    sources: Vec<PeriodicScalarSource>,
}

impl PeriodicScalarSources {
    fn reset(&mut self) {
        self.sources.clear();
    }
}

static PERIODIC_SCALAR_SOURCES: LazyLock<Mutex<PeriodicScalarSources>> =
    LazyLock::new(|| Mutex::new(PeriodicScalarSources::default()));

/// Add a periodic scalar producer to any Rig dataflow runtime.
pub fn add_periodic_scalar_source(
    runtime: &mut dyn DataflowRuntime,
    node: u32,
    route_id: u32,
    period_ns: u64,
    reader: ScalarSourceReader,
) -> bool {
    if !runtime.node_exists(node) || route_id == 0 || period_ns == 0 {
        return false;
    }
    let mut sources = PERIODIC_SCALAR_SOURCES.lock().unwrap();
    if sources
        .sources
        .iter()
        .any(|source| source.node == node && source.route_id == route_id)
    {
        return true;
    }
    let source = PeriodicScalarSource {
        node,
        route_id,
        period_ns,
        last_emit_ns: runtime.elapsed_ns(),
        reader,
    };
    let source_index = sources.sources.len();
    sources.sources.push(source);
    drop(sources);

    let registered = algorithms::register_algorithm(
        runtime,
        DataflowAlgorithm::periodic_source(
            node,
            (node, 1, source_index),
            vec![scalar::edge(node, route_id)],
            Arc::new(PeriodicScalarSourceAlgorithm { source_index }),
            period_ns,
            source.due_at_ns(),
        )
        .with_runtime_reset(reset_periodic_scalar_sources)
        .with_scalar_source(node, route_id, source_index, take_periodic_scalar_events),
    );
    if !registered {
        PERIODIC_SCALAR_SOURCES.lock().unwrap().sources.pop();
    }
    registered
}

fn reset_periodic_scalar_sources() {
    PERIODIC_SCALAR_SOURCES.lock().unwrap().reset();
}

fn reset_scalar_source_banks() {
    SCALAR_SOURCE_BANKS.lock().unwrap().reset();
}

/// Register a mutable scalar route in a Rust-owned source bank.
///
/// Routes sharing a node and period share the bank, but each route has its
/// own value and event queue. Registration is idempotent so native datapath
/// discovery can call it more than once while a graph is being connected.
pub fn add_scalar_source_bank_route(
    runtime: &mut dyn DataflowRuntime,
    node: u32,
    route_id: u32,
    period_ns: u64,
    initial_value: f32,
) -> bool {
    if !runtime.node_exists(node) || route_id == 0 || period_ns == 0 || !initial_value.is_finite() {
        return false;
    }

    let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let bank_index = if let Some(index) = banks
        .banks
        .iter()
        .position(|bank| bank.node == node && bank.period_ns == period_ns)
    {
        index
    } else {
        banks.banks.push(ScalarSourceBank {
            node,
            period_ns,
            channels: Vec::new(),
        });
        banks.banks.len() - 1
    };

    let bank = &mut banks.banks[bank_index];
    if let Some(channel_index) = bank
        .channels
        .iter()
        .position(|channel| channel.route_id == route_id)
    {
        bank.channels[channel_index].value = initial_value;
        return true;
    }

    let channel_index = bank.channels.len();
    bank.channels.push(ScalarSourceChannel {
        route_id,
        value: initial_value,
        pending: VecDeque::new(),
    });
    banks.channel_refs.push(ScalarSourceChannelRef {
        bank_index,
        channel_index,
    });
    drop(banks);

    let Some(algorithm) = scalar_source_bank_algorithm(bank_index) else {
        let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
        banks.channel_refs.pop();
        if let Some(bank) = banks.banks.get_mut(bank_index) {
            bank.channels.pop();
            if bank.channels.is_empty() {
                banks.banks.pop();
            }
        }
        return false;
    };
    let registered = algorithms::replace_algorithm(runtime, algorithm);
    if !registered {
        let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
        banks.channel_refs.pop();
        if let Some(bank) = banks.banks.get_mut(bank_index) {
            bank.channels.pop();
            if bank.channels.is_empty() {
                banks.banks.pop();
            }
        }
    }
    registered
}

/// Update a configured route. The next native periodic emission carries the
/// new value and its Rust-owned scheduler timestamp.
pub fn set_scalar_source_bank_value(node: u32, route_id: u32, value: f32) -> bool {
    if !value.is_finite() {
        return false;
    }
    let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let Some(channel) = banks
        .banks
        .iter_mut()
        .filter(|bank| bank.node == node)
        .find_map(|bank| {
            bank.channels
                .iter_mut()
                .find(|channel| channel.route_id == route_id)
        })
    else {
        return false;
    };
    channel.value = value;
    true
}

/// Publish one complete source-bank sample without entering the runtime
/// mutex. This function is intentionally reentrant: Python model callbacks
/// invoke it while the Rust scheduler is already running the cluster.
pub fn publish_scalar_source_bank_events(
    node: u32,
    period_ns: u64,
    timestamp_ns: u64,
    route_ids: &[u32],
    values: &[f32],
) -> bool {
    if route_ids.is_empty()
        || route_ids.len() != values.len()
        || route_ids.iter().any(|route_id| *route_id == 0)
        || values.iter().any(|value| !value.is_finite())
    {
        return false;
    }
    let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let Some(bank) = banks
        .banks
        .iter_mut()
        .find(|bank| bank.node == node && bank.period_ns == period_ns)
    else {
        return false;
    };
    if route_ids.iter().any(|route_id| {
        !bank
            .channels
            .iter()
            .any(|channel| channel.route_id == *route_id)
    }) {
        return false;
    }
    for (route_id, value) in route_ids.iter().copied().zip(values.iter().copied()) {
        let channel = bank
            .channels
            .iter_mut()
            .find(|channel| channel.route_id == route_id)
            .expect("source bank route was prevalidated");
        channel.value = value;
    }
    for (route_id, value) in route_ids.iter().copied().zip(values.iter().copied()) {
        let Some(channel) = bank
            .channels
            .iter_mut()
            .find(|channel| channel.route_id == route_id)
        else {
            return false;
        };
        channel.pending.push_back(ScalarEvent {
            value,
            timestamp_ns,
        });
    }
    true
}

fn take_scalar_source_bank_event(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let Some(source) = banks.channel_refs.get(context).copied() else {
        return Vec::new();
    };
    let Some(bank) = banks.banks.get_mut(source.bank_index) else {
        return Vec::new();
    };
    let Some(channel) = bank.channels.get_mut(source.channel_index) else {
        return Vec::new();
    };
    let _ = elapsed_ns;
    channel.pending.pop_front().into_iter().collect()
}

struct ScalarSourceBankAlgorithm {
    bank_index: usize,
}

fn scalar_source_bank_algorithm(bank_index: usize) -> Option<DataflowAlgorithm> {
    let banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let bank = banks.banks.get(bank_index)?;
    let outputs = bank
        .channels
        .iter()
        .map(|channel| scalar::edge(bank.node, channel.route_id))
        .collect();
    let mut algorithm = DataflowAlgorithm::source(
        bank.node,
        (bank.node, 2, bank_index),
        outputs,
        Arc::new(ScalarSourceBankAlgorithm { bank_index }),
    )
    .with_runtime_reset(reset_scalar_source_banks);
    for (source_index, source) in banks.channel_refs.iter().enumerate() {
        if source.bank_index != bank_index {
            continue;
        }
        let route_id = bank.channels[source.channel_index].route_id;
        algorithm = algorithm.with_scalar_source(
            bank.node,
            route_id,
            source_index,
            take_scalar_source_bank_event,
        );
    }
    Some(algorithm)
}

impl DataflowAlgorithmExecutor for ScalarSourceBankAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        let banks = SCALAR_SOURCE_BANKS.lock().unwrap();
        let Some(bank) = banks.banks.get(self.bank_index) else {
            return false;
        };
        runtime.node_online(bank.node)
            && bank
                .channels
                .iter()
                .any(|channel| !channel.pending.is_empty())
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let Some((node, events)) = take_scalar_source_bank_events(self.bank_index) else {
            return false;
        };
        for (route_id, event) in events {
            scalar::route_native_event(runtime, node, route_id, event);
        }
        true
    }
}

fn take_scalar_source_bank_events(bank_index: usize) -> Option<(u32, Vec<(u32, ScalarEvent)>)> {
    let mut banks = SCALAR_SOURCE_BANKS.lock().unwrap();
    let bank = banks.banks.get_mut(bank_index)?;
    let events = bank
        .channels
        .iter_mut()
        .filter_map(|channel| {
            channel
                .pending
                .pop_front()
                .map(|event| (channel.route_id, event))
        })
        .collect();
    Some((bank.node, events))
}

fn take_periodic_scalar_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    PERIODIC_SCALAR_SOURCES
        .lock()
        .unwrap()
        .sources
        .get_mut(context)
        .and_then(|source| source.emit_if_due(elapsed_ns))
        .into_iter()
        .collect()
}

struct PeriodicScalarSourceAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for PeriodicScalarSourceAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        PERIODIC_SCALAR_SOURCES
            .lock()
            .unwrap()
            .sources
            .get(self.source_index)
            .is_some_and(|source| {
                runtime.node_online(source.node) && source.has_pending_event(runtime.elapsed_ns())
            })
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let event = take_periodic_scalar_events(self.source_index, runtime.elapsed_ns())
            .into_iter()
            .next();
        let Some(event) = event else {
            return false;
        };
        let sources = PERIODIC_SCALAR_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get(self.source_index) else {
            return false;
        };
        scalar::route_native_event(runtime, source.node, source.route_id, event);
        true
    }
}

#[cfg(test)]
mod tests {
    use super::super::runtime::{NoBackend, RigRuntime};
    use super::*;
    use std::sync::{Mutex, MutexGuard};

    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn test_lock() -> MutexGuard<'static, ()> {
        TEST_LOCK.lock().unwrap()
    }

    fn read_value() -> f32 {
        12.5
    }

    #[test]
    fn periodic_scalar_sources_are_generic_rig_algorithms() {
        let _lock = test_lock();
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);

        assert!(add_periodic_scalar_source(
            &mut runtime,
            node,
            7,
            10,
            read_value,
        ));
        assert_eq!(runtime.run_for_ns(9, 100), 9);
        assert!(runtime.latest_scalar_event(node, 7).is_none());
        assert_eq!(runtime.run_for_ns(1, 100), 1);
        assert_eq!(runtime.latest_scalar_event(node, 7).unwrap().value, 12.5);
    }

    #[test]
    fn periodic_scalar_sources_reject_zero_periods() {
        let _lock = test_lock();
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);
        assert!(!add_periodic_scalar_source(
            &mut runtime,
            node,
            7,
            0,
            read_value,
        ));
    }

    #[test]
    fn scalar_source_bank_owns_values_and_event_timestamps() {
        let _lock = test_lock();
        reset_scalar_source_banks();
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);

        assert!(add_scalar_source_bank_route(
            &mut runtime,
            node,
            11,
            10,
            1.0,
        ));
        assert!(add_scalar_source_bank_route(
            &mut runtime,
            node,
            12,
            10,
            2.0,
        ));
        assert!(set_scalar_source_bank_value(node, 11, 3.5));

        assert_eq!(runtime.run_for_ns(9, 100), 9);
        assert!(runtime.latest_scalar_event(node, 11).is_none());
        assert!(publish_scalar_source_bank_events(
            node,
            10,
            9,
            &[11, 12],
            &[3.5, 2.0],
        ));
        assert_eq!(runtime.run_for_ns(1, 100), 1);

        let first = runtime.latest_scalar_event(node, 11).unwrap();
        let second = runtime.latest_scalar_event(node, 12).unwrap();
        assert_eq!(first.value, 3.5);
        assert_eq!(first.timestamp_ns, 9);
        assert_eq!(second.value, 2.0);
        assert_eq!(second.timestamp_ns, 9);

        assert!(set_scalar_source_bank_value(node, 12, 4.5));
        assert!(publish_scalar_source_bank_events(
            node,
            10,
            10,
            &[11, 12],
            &[3.5, 4.5],
        ));
        assert_eq!(runtime.run_for_ns(1, 100), 1);
        let second = runtime.latest_scalar_event(node, 12).unwrap();
        assert_eq!(second.value, 4.5);
        assert_eq!(second.timestamp_ns, 10);
    }

    #[test]
    fn scalar_source_bank_rejects_invalid_configuration() {
        let _lock = test_lock();
        reset_scalar_source_banks();
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);

        assert!(!add_scalar_source_bank_route(
            &mut runtime,
            node,
            11,
            0,
            1.0,
        ));
        assert!(!add_scalar_source_bank_route(
            &mut runtime,
            node,
            11,
            10,
            f32::NAN,
        ));
        assert!(!set_scalar_source_bank_value(node, 11, 1.0));
    }

    #[test]
    fn scalar_source_bank_rejects_partial_batches_before_mutating_values() {
        let _lock = test_lock();
        reset_scalar_source_banks();
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);

        assert!(add_scalar_source_bank_route(
            &mut runtime,
            node,
            11,
            10,
            1.0
        ));
        assert!(publish_scalar_source_bank_events(
            node,
            10,
            1,
            &[11],
            &[2.0]
        ));
        assert!(!publish_scalar_source_bank_events(
            node,
            10,
            2,
            &[11, 99],
            &[3.0, 4.0],
        ));

        assert_eq!(runtime.run_for_ns(1, 100), 1);
        let event = runtime.latest_scalar_event(node, 11).unwrap();
        assert_eq!(event.value, 2.0);
        assert_eq!(event.timestamp_ns, 1);
    }
}
