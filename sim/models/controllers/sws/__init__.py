from enum import Enum

from sim.bindings.firmware.timer import TimerInterface
from rig import extend_model_class, load_generated_enums, load_generated_module

from .simple import SwsSimpleModel


class SwsButton(Enum):
    """Physical SWS button inputs.

    The model presents these as logical pressed states while the firmware
    binding continues to receive the active-low GPIO levels used by the real
    steering wheel hardware.
    """

    LEFT_TOP = "left_top"
    LEFT_MID = "left_mid"
    LEFT_BOT = "left_bot"
    RIGHT_TOP = "right_top"
    RIGHT_MID = "right_mid"
    RIGHT_BOT = "right_bot"
    LEFT_TOGGLE = "left_toggle"
    RIGHT_TOGGLE = "right_toggle"


class SwsRequest(Enum):
    """Driver requests that can be produced by physical SWS inputs."""

    RUN = "run"
    REVERSE = "reverse"
    RACE = "race"
    LAUNCH_CONTROL = "launch_control"
    TORQUE_DEC = "torque_dec"
    TORQUE_INC = "torque_inc"
    SLIP_DEC = "slip_dec"
    SLIP_INC = "slip_inc"
    TRACTION_CONTROL = "traction_control"
    REGEN = "regen"


_BUTTON_PINS = {
    SwsButton.LEFT_TOP: "DIN1",
    SwsButton.LEFT_MID: "DIN2",
    SwsButton.LEFT_BOT: "DIN3",
    SwsButton.RIGHT_TOP: "DIN4",
    SwsButton.RIGHT_MID: "DIN5",
    SwsButton.RIGHT_BOT: "DIN6",
    SwsButton.LEFT_TOGGLE: "DIN7",
    SwsButton.RIGHT_TOGGLE: "DIN8",
}

# These combinations mirror driverInput.c. Keeping the mapping here makes
# test intent readable without replacing the firmware's debounce/combo logic.
_REQUEST_BUTTONS = {
    SwsRequest.RUN: (SwsButton.LEFT_TOP, SwsButton.RIGHT_TOP),
    SwsRequest.REVERSE: (
        SwsButton.LEFT_BOT,
        SwsButton.RIGHT_BOT,
        SwsButton.LEFT_MID,
        SwsButton.RIGHT_MID,
    ),
    SwsRequest.RACE: (SwsButton.LEFT_BOT, SwsButton.RIGHT_BOT),
    SwsRequest.LAUNCH_CONTROL: (SwsButton.LEFT_MID, SwsButton.RIGHT_MID),
    SwsRequest.TORQUE_DEC: (SwsButton.LEFT_MID,),
    SwsRequest.TORQUE_INC: (SwsButton.RIGHT_MID,),
    SwsRequest.SLIP_DEC: (SwsButton.LEFT_BOT,),
    SwsRequest.SLIP_INC: (SwsButton.RIGHT_BOT,),
    SwsRequest.TRACTION_CONTROL: (SwsButton.LEFT_TOGGLE,),
    SwsRequest.REGEN: (SwsButton.RIGHT_TOGGLE,),
}


def _load_generated() -> None:
    if "SwsModel" in globals():
        return

    model = load_generated_module(
        "SWS_MODEL_PY",
        "//sim/models/controllers/sws:sws-py",
        "sws_generated_model",
    )
    enums = _load_generated_enums()

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalInput"] = enums.DigitalInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["DigitalStatus"] = enums.DigitalStatus
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort

    class SwsModel(extend_model_class(model.SwsModel)):
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)

        def __init__(self, *args, **kwargs):
            self._buttons: dict[SwsButton, bool] = {
                button: False for button in SwsButton
            }
            self._requests: dict[SwsRequest, bool] = {
                request: False for request in SwsRequest
            }
            super().__init__(*args, **kwargs)

        def reset(self) -> None:
            super().reset()
            self._buttons = {button: False for button in SwsButton}
            self._requests = {request: False for request in SwsRequest}
            self._apply_buttons()

        @staticmethod
        def _coerce_button(button: SwsButton | str) -> SwsButton:
            if isinstance(button, SwsButton):
                return button
            try:
                return SwsButton(button)
            except ValueError as exc:
                valid = ", ".join(item.value for item in SwsButton)
                raise ValueError(
                    f"unknown SWS button {button!r}; expected one of {valid}"
                ) from exc

        @staticmethod
        def _coerce_request(request: SwsRequest | str) -> SwsRequest:
            if isinstance(request, SwsRequest):
                return request
            try:
                return SwsRequest(request)
            except ValueError as exc:
                valid = ", ".join(item.value for item in SwsRequest)
                raise ValueError(
                    f"unknown SWS request {request!r}; expected one of {valid}"
                ) from exc

        def _apply_buttons(self) -> None:
            requested = {
                button
                for request, asserted in self._requests.items()
                if asserted
                for button in _REQUEST_BUTTONS[request]
            }
            for button, explicitly_pressed in self._buttons.items():
                pin = getattr(DigitalIo, _BUTTON_PINS[button])
                # SWS input GPIOs are active-low in the firmware hardware map.
                self.set_digital_io(
                    pin, not (explicitly_pressed or button in requested)
                )

        def set_button(self, button: SwsButton | str, pressed: bool) -> None:
            """Set one physical button and let firmware derive its requests."""
            self._buttons[self._coerce_button(button)] = bool(pressed)
            self._apply_buttons()

        def press_button(self, button: SwsButton | str) -> None:
            self.set_button(button, True)

        def release_button(self, button: SwsButton | str) -> None:
            self.set_button(button, False)

        def set_request(self, request: SwsRequest | str, asserted: bool) -> None:
            """Hold or release the physical input combination for a request.

            This is a test convenience only: the request is still generated by
            the firmware's debounce and driver-input state machine.
            """
            self._requests[self._coerce_request(request)] = bool(asserted)
            self._apply_buttons()

        def assert_request(self, request: SwsRequest | str) -> None:
            self.set_request(request, True)

        def clear_request(self, request: SwsRequest | str) -> None:
            self.set_request(request, False)

    globals()["SwsModel"] = SwsModel


def _load_generated_enums():
    return load_generated_enums(
        "SWS_ENUMS_PY",
        "//sim/models/controllers/sws:enums-py",
        "sws_generated_enums",
        globals(),
    )


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "SWS_CLUSTERS":
        from .variants import SWS_CLUSTERS

        globals()["SWS_CLUSTERS"] = SWS_CLUSTERS
        return SWS_CLUSTERS
    _load_generated_enums()
    if name in globals():
        return globals()[name]
    if name in _GENERATED_EXPORTS:
        _load_generated()
        return globals()[name]
    raise AttributeError(name)


_GENERATED_EXPORTS = {
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "DigitalOutput",
    "DigitalStatus",
    "TimerChannel",
    "TimerPort",
    "SwsModel",
    "SWS_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "DigitalOutput",
    "DigitalStatus",
    "TimerChannel",
    "TimerPort",
    "PLATFORM_VARIANTS",
    "SWS_CLUSTERS",
    "SwsSimpleModel",
    "SwsModel",
    "SwsButton",
    "SwsRequest",
]
