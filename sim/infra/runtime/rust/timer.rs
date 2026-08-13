use std::sync::{LazyLock, Mutex};

use super::datapath::{DataPath, DataPathEvent};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimerChannel {
    pub port: i32,
    pub channel: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerChannelEvent {
    pub port: i32,
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

impl TimerChannelEvent {
    fn timer_channel(&self) -> TimerChannel {
        TimerChannel {
            port: self.port,
            channel: self.channel,
        }
    }
}

impl DataPathEvent for TimerChannelEvent {
    type Channel = TimerChannel;

    fn channel(&self) -> Self::Channel {
        self.timer_channel()
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimerCaptureChannel {
    pub channel: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerCaptureEvent {
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

impl TimerCaptureEvent {
    fn timer_channel(&self) -> TimerCaptureChannel {
        TimerCaptureChannel {
            channel: self.channel,
        }
    }
}

impl DataPathEvent for TimerCaptureEvent {
    type Channel = TimerCaptureChannel;

    fn channel(&self) -> Self::Channel {
        self.timer_channel()
    }
}

#[derive(Debug)]
struct TimerPeripheral {
    channel: TimerChannel,
    duty_inputs: DataPath<TimerChannelEvent>,
    duty_outputs: DataPath<TimerChannelEvent>,
    frequency_inputs: DataPath<TimerChannelEvent>,
    frequency_outputs: DataPath<TimerChannelEvent>,
}

impl TimerPeripheral {
    fn new(channel: TimerChannel) -> Self {
        Self {
            channel,
            duty_inputs: DataPath::new(channel),
            duty_outputs: DataPath::new(channel),
            frequency_inputs: DataPath::new(channel),
            frequency_outputs: DataPath::new(channel),
        }
    }

    fn reset(&mut self) {
        self.duty_inputs.clear();
        self.duty_outputs.clear();
        self.frequency_inputs.clear();
        self.frequency_outputs.clear();
    }
}

#[derive(Debug)]
struct TimerCapturePeripheral {
    capture_inputs: DataPath<TimerCaptureEvent>,
}

impl TimerCapturePeripheral {
    fn new(channel: TimerCaptureChannel) -> Self {
        Self {
            capture_inputs: DataPath::new(channel),
        }
    }

    fn reset(&mut self) {
        self.capture_inputs.clear();
    }
}

#[derive(Debug, Default)]
struct TimerModel {
    peripherals: Vec<TimerPeripheral>,
    capture_peripherals: Vec<TimerCapturePeripheral>,
}

impl TimerModel {
    fn reset(&mut self) {
        for peripheral in &mut self.peripherals {
            peripheral.reset();
        }
        for peripheral in &mut self.capture_peripherals {
            peripheral.reset();
        }
    }

    fn peripheral(&mut self, channel: TimerChannel) -> &mut TimerPeripheral {
        if let Some(index) = self
            .peripherals
            .iter()
            .position(|peripheral| peripheral.channel == channel)
        {
            return &mut self.peripherals[index];
        }

        self.peripherals.push(TimerPeripheral::new(channel));
        self.peripherals.last_mut().unwrap()
    }

    fn capture_peripheral(&mut self, channel: TimerCaptureChannel) -> &mut TimerCapturePeripheral {
        if let Some(index) = self
            .capture_peripherals
            .iter()
            .position(|peripheral| peripheral.capture_inputs.channel() == channel)
        {
            return &mut self.capture_peripherals[index];
        }

        self.capture_peripherals
            .push(TimerCapturePeripheral::new(channel));
        self.capture_peripherals.last_mut().unwrap()
    }
}

static TIMER_MODEL: LazyLock<Mutex<TimerModel>> =
    LazyLock::new(|| Mutex::new(TimerModel::default()));

pub fn reset() {
    TIMER_MODEL.lock().unwrap().reset();
}

pub fn configure_channel(port: i32, channel: i32) {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel });
}

pub fn push_duty_input(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .duty_inputs
        .push(event)
}

pub fn push_duty_inputs(events: &[TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let mut count = 0;
    for event in events {
        if timer
            .peripheral(event.timer_channel())
            .duty_inputs
            .push(*event)
        {
            count += 1;
        }
    }
    count
}

pub fn latest_duty_input(port: i32, channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .peripheral(TimerChannel { port, channel })
        .duty_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_frequency_input(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .frequency_inputs
        .push(event)
}

pub fn push_frequency_inputs(events: &[TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let mut count = 0;
    for event in events {
        if timer
            .peripheral(event.timer_channel())
            .frequency_inputs
            .push(*event)
        {
            count += 1;
        }
    }
    count
}

pub fn latest_frequency_input(port: i32, channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .peripheral(TimerChannel { port, channel })
        .frequency_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_capture_input(event: TimerCaptureEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .capture_peripheral(event.timer_channel())
        .capture_inputs
        .push(event)
}

pub fn latest_capture_input(channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .capture_peripheral(TimerCaptureChannel { channel })
        .capture_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_duty_output(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .duty_outputs
        .push(event)
}

pub fn pop_duty_output(port: i32, channel: i32) -> Option<TimerChannelEvent> {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .duty_outputs
        .pop()
}

pub fn pop_duty_outputs(port: i32, channel: i32, out: &mut [TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let output = &mut timer
        .peripheral(TimerChannel { port, channel })
        .duty_outputs;
    let mut count = 0;
    for slot in out.iter_mut() {
        let Some(event) = output.pop() else {
            break;
        };
        *slot = event;
        count += 1;
    }
    count
}

pub fn duty_output_count(port: i32, channel: i32) -> u32 {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .duty_outputs
        .count()
}

pub fn push_frequency_output(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .frequency_outputs
        .push(event)
}

pub fn pop_frequency_output(port: i32, channel: i32) -> Option<TimerChannelEvent> {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs
        .pop()
}

pub fn pop_frequency_outputs(port: i32, channel: i32, out: &mut [TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let output = &mut timer
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs;
    let mut count = 0;
    for slot in out.iter_mut() {
        let Some(event) = output.pop() else {
            break;
        };
        *slot = event;
        count += 1;
    }
    count
}

pub fn frequency_output_count(port: i32, channel: i32) -> u32 {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs
        .count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_input(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_duty_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_inputs(
    events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts(events, count as usize) };
    push_duty_inputs(events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_duty_input(
    port: i32,
    channel: i32,
    value: *mut f32,
) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_duty_input(port, channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_input(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_frequency_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_inputs(
    events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts(events, count as usize) };
    push_frequency_inputs(events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_frequency_input(
    port: i32,
    channel: i32,
    value: *mut f32,
) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_frequency_input(port, channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_capture_input(event: *const TimerCaptureEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_capture_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_capture_input(channel: i32, value: *mut f32) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_capture_input(channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_output(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_duty_output(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_duty_output(
    port: i32,
    channel: i32,
    event: *mut TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match pop_duty_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_duty_outputs(
    port: i32,
    channel: i32,
    events: *mut TimerChannelEvent,
    capacity: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts_mut(events, capacity as usize) };
    pop_duty_outputs(port, channel, events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_duty_output_count(port: i32, channel: i32) -> u32 {
    duty_output_count(port, channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_output(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_frequency_output(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_frequency_output(
    port: i32,
    channel: i32,
    event: *mut TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match pop_frequency_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_frequency_outputs(
    port: i32,
    channel: i32,
    events: *mut TimerChannelEvent,
    capacity: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts_mut(events, capacity as usize) };
    pop_frequency_outputs(port, channel, events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_frequency_output_count(port: i32, channel: i32) -> u32 {
    frequency_output_count(port, channel)
}
