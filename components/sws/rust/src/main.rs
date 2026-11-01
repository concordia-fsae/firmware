#![no_std]
#![no_main]

use core::panic::PanicInfo;
#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    cortex_m::interrupt::disable();
    loop {
        cortex_m::asm::nop();
    }
}

#[rtic::app(device = firmware_shared::stm32f1::hal::pac)]
mod firmware {
    use firmware_shared::stm32f1::{
        can::{Bus, TxQueue},
        clock,
        gpio::ActiveLowInputs,
        hal::{
            can::Can,
            gpio::{gpioa::PA8, gpioc::PC13, OpenDrain, Output, PushPull},
            pac,
            prelude::*,
            timer::{CounterMs, Event},
        },
    };
    use sws::{app::App, protocol::*, uds::Descriptor};
    #[shared]
    struct Shared {
        bus: Bus<32>,
    }
    #[local]
    struct Local {
        timer: CounterMs<pac::TIM2>,
        led: PC13<Output<PushPull>>,
        _can_sleep: PA8<Output<OpenDrain>>,
        inputs: ActiveLowInputs<8>,
        app: App,
        descriptor: Descriptor,
        tx: TxQueue<32>,
    }
    #[init]
    fn init(mut cx: init::Context) -> (Shared, Local) {
        // The bootloader jumps directly to Reset, so relocate the vector table.
        unsafe {
            clock::initialize_core(&mut cx.core, 0x0800_2100);
        }
        let clocks = clock::configure(
            cx.device.FLASH,
            cx.device.RCC,
            clock::Config {
                hse_mhz: 8,
                sysclk_mhz: 64,
                pclk1_mhz: 32,
                pclk2_mhz: 64,
            },
        );
        let mut afio = cx.device.AFIO.constrain();
        let mut gpioa = cx.device.GPIOA.split();
        let mut gpiob = cx.device.GPIOB.split();
        let mut gpioc = cx.device.GPIOC.split();
        let led = gpioc.pc13.into_push_pull_output(&mut gpioc.crh);
        // PA8 is active low, open-drain: enable the external CAN transceiver.
        let mut can_sleep = gpioa.pa8.into_open_drain_output(&mut gpioa.crh);
        can_sleep.set_low();
        let inputs = ActiveLowInputs::new([
            gpiob.pb5.into_floating_input(&mut gpiob.crl).erase(),
            gpiob.pb7.into_floating_input(&mut gpiob.crl).erase(),
            gpiob.pb8.into_floating_input(&mut gpiob.crh).erase(),
            gpiob.pb9.into_floating_input(&mut gpiob.crh).erase(),
            gpiob.pb10.into_floating_input(&mut gpiob.crh).erase(),
            gpiob.pb11.into_floating_input(&mut gpiob.crh).erase(),
            gpiob.pb12.into_floating_input(&mut gpiob.crh).erase(),
            gpiob.pb13.into_floating_input(&mut gpiob.crh).erase(),
        ]);
        let can = Can::new(cx.device.CAN1, cx.device.USB);
        can.assign_pins(
            (
                gpioa.pa12.into_alternate_push_pull(&mut gpioa.crh),
                gpioa.pa11.into_floating_input(&mut gpioa.crh),
            ),
            &mut afio.mapr,
        );
        let bus = Bus::new(
            can,
            0x0005_0003,
            MESSAGES[..MSG_SWS_FAULTS].iter().map(|m| m.id),
        );
        let mut timer = cx.device.TIM2.counter_ms(&clocks);
        timer.start(1.millis()).unwrap();
        timer.listen(Event::Update);
        let descriptor = unsafe { Descriptor::read_flash(0x0800_2000) };
        (
            Shared { bus },
            Local {
                timer,
                led,
                _can_sleep: can_sleep,
                inputs,
                app: App::default(),
                descriptor,
                tx: TxQueue::new(MESSAGES[MSG_SWS_UDS_RESPONSE].id),
            },
        )
    }
    #[task(binds = USB_LP_CAN_RX0, priority = 2, shared = [bus])]
    fn can_rx(mut cx: can_rx::Context) {
        cx.shared.bus.lock(|bus| bus.receive());
    }

    #[task(binds = TIM2, priority = 1, shared = [bus], local = [timer, led, inputs, app, descriptor, tx])]
    fn tick(mut cx: tick::Context) {
        let started = clock::cycles();
        cx.local.timer.clear_interrupt(Event::Update);
        cx.local.app.timer_task(
            *cx.local.descriptor,
            || cx.shared.bus.lock(|bus| bus.pop_received()),
            || cx.local.inputs.sample(),
            |frame| cx.local.tx.enqueue(frame),
            || cx.local.led.toggle(),
        );
        cx.local
            .tx
            .flush(|frame| cx.shared.bus.lock(|bus| bus.transmit(frame)));
        cx.local.app.runtime_cycles += clock::elapsed_cycles(started);
        if cx.local.app.diagnostics.reset_due(cx.local.app.now) {
            cortex_m::peripheral::SCB::sys_reset();
        }
    }
    #[idle]
    fn idle(_: idle::Context) -> ! {
        loop {
            cortex_m::asm::wfi();
        }
    }
}
