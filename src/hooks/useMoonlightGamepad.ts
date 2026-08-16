import {useCallback, useRef} from 'react';
import {
  type GamepadEvent,
  useGamepadEventHandler,
} from '@amazon-devices/react-native-kepler';
import {moonlightService} from '../services/moonlight';

const FLAGS: Record<string, number> = {
  action_btn_a: 0x1000,
  action_btn_b: 0x2000,
  action_btn_x: 0x4000,
  action_btn_y: 0x8000,
  dpad_btn_up: 0x0001,
  dpad_btn_down: 0x0002,
  dpad_btn_left: 0x0004,
  dpad_btn_right: 0x0008,
  left_upper_trigger_btn_tl: 0x0100,
  right_upper_trigger_btn_tr: 0x0200,
  right_menu_btn_start: 0x0010,
  left_menu_btn_select: 0x0020,
  left_stick_btn_thumbl: 0x0040,
  right_stick_btn_thumbr: 0x0080,
};

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
      const values = event.axis;
      state.current.leftStickX = axis(values.left_stick_x);
      state.current.leftStickY = axis(values.left_stick_y, true);
      state.current.rightStickX = axis(values.right_stick_x);
      state.current.rightStickY = axis(values.right_stick_y, true);
      state.current.leftTrigger = trigger(values.left_lower_trigger);
      state.current.rightTrigger = trigger(values.right_lower_trigger);
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
