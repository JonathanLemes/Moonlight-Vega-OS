import {useCallback, useRef} from 'react';
import {
  type GamepadEvent,
  useGamepadEventHandler,
} from '@amazon-devices/react-native-kepler';
import {moonlightService} from '../services/moonlight';

// Vega only normalizes controllers it specifically recognizes into the
// semantic "action_btn_a" style names below. Controllers it doesn't (the
// DualSense included, confirmed via device log capture on 2026-08-17) fall
// back to raw Linux evdev BTN_*/ABS_* names instead — a different naming
// scheme entirely, not just a variant. Both are mapped here so either kind
// of pad works. See docs/2026-08-17 project history for the captured event
// dump this mapping was built from.
const FLAGS: Record<string, number> = {
  action_btn_a: 0x1000,
  btn_a: 0x1000,
  action_btn_b: 0x2000,
  btn_b: 0x2000,
  action_btn_x: 0x4000,
  btn_x: 0x4000,
  action_btn_y: 0x8000,
  btn_y: 0x8000,
  dpad_btn_up: 0x0001,
  dpad_btn_down: 0x0002,
  dpad_btn_left: 0x0004,
  dpad_btn_right: 0x0008,
  left_upper_trigger_btn_tl: 0x0100,
  btn_tl: 0x0100,
  right_upper_trigger_btn_tr: 0x0200,
  btn_tr: 0x0200,
  right_menu_btn_start: 0x0010,
  btn_start: 0x0010,
  left_menu_btn_select: 0x0020,
  btn_select: 0x0020,
  left_stick_btn_thumbl: 0x0040,
  btn_thumbl: 0x0040,
  right_stick_btn_thumbr: 0x0080,
  btn_thumbr: 0x0080,
};

const DPAD_UP = 0x0001;
const DPAD_DOWN = 0x0002;
const DPAD_LEFT = 0x0004;
const DPAD_RIGHT = 0x0008;
const DPAD_MASK = DPAD_UP | DPAD_DOWN | DPAD_LEFT | DPAD_RIGHT;

interface State {
  buttons: number;
  leftTrigger: number;
  rightTrigger: number;
  leftStickX: number;
  leftStickY: number;
  rightStickX: number;
  rightStickY: number;
}

const axis = (value: number | undefined, invert = false) => {
  const normalized = Math.max(-1, Math.min(1, value ?? 0));
  return Math.round(normalized * (invert ? -32767 : 32767));
};

const trigger = (value: number | undefined) =>
  Math.round(Math.max(0, Math.min(1, value ?? 0)) * 255);

export const useMoonlightGamepad = () => {
  const state = useRef<State>({
    buttons: 0,
    leftTrigger: 0,
    rightTrigger: 0,
    leftStickX: 0,
    leftStickY: 0,
    rightStickX: 0,
    rightStickY: 0,
  });

  const send = useCallback(() => {
    const value = state.current;
    moonlightService.sendControllerState(
      value.buttons,
      value.leftTrigger,
      value.rightTrigger,
      value.leftStickX,
      value.leftStickY,
      value.rightStickX,
      value.rightStickY,
    );
  }, []);

  useGamepadEventHandler((event: GamepadEvent) => {
    if (event.eventType === 'axis' && event.axis) {
      const values = event.axis as Record<string, number | undefined>;
      state.current.leftStickX = axis(values.left_stick_x ?? values.ABS_X);
      state.current.leftStickY = axis(
        values.left_stick_y ?? values.ABS_Y,
        true,
      );
      state.current.rightStickX = axis(values.right_stick_x ?? values.ABS_RX);
      state.current.rightStickY = axis(
        values.right_stick_y ?? values.ABS_RY,
        true,
      );
      // Raw-evdev pads (DualSense included) report L2/R2 pull as ABS_HAT2Y
      // and ABS_HAT2X respectively — the driver reuses the otherwise-unused
      // extra hat-switch axis slots for the trigger analog values.
      state.current.leftTrigger = trigger(
        values.left_lower_trigger ?? values.ABS_HAT2Y,
      );
      state.current.rightTrigger = trigger(
        values.right_lower_trigger ?? values.ABS_HAT2X,
      );

      // Same story for the D-pad: it arrives as a hat axis instead of four
      // discrete buttons, so rebuild the dpad bits every time it moves.
      if (values.ABS_HAT0X !== undefined || values.ABS_HAT0Y !== undefined) {
        let dpad = 0;
        if ((values.ABS_HAT0Y ?? 0) < 0) dpad |= DPAD_UP;
        if ((values.ABS_HAT0Y ?? 0) > 0) dpad |= DPAD_DOWN;
        if ((values.ABS_HAT0X ?? 0) < 0) dpad |= DPAD_LEFT;
        if ((values.ABS_HAT0X ?? 0) > 0) dpad |= DPAD_RIGHT;
        state.current.buttons = (state.current.buttons & ~DPAD_MASK) | dpad;
      }

      send();
      return;
    }
    const flag = FLAGS[event.eventType];
    if (flag === undefined) {
      return;
    }
    if (event.eventKeyAction === 0) {
      state.current.buttons |= flag;
    } else if (event.eventKeyAction === 1) {
      state.current.buttons &= ~flag;
    }
    send();
  });
};
