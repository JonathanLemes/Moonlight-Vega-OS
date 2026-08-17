import React, {useEffect, useRef, useState} from 'react';
import {Pressable, StyleSheet, Text, View} from 'react-native';

interface FocusedButtonProps {
  label: string;
  disabled?: boolean;
  preferredFocus?: boolean;
  onPress: () => void;
}

export const FocusedButton = ({
  label,
  disabled = false,
  preferredFocus = false,
  onPress,
}: FocusedButtonProps) => {
  const [focused, setFocused] = useState(false);
  const buttonRef = useRef<View & {requestTVFocus(): void}>(null);

  useEffect(() => {
    if (!preferredFocus) {
      return;
    }
    const frame = requestAnimationFrame(() => {
      buttonRef.current?.requestTVFocus();
    });
    return () => cancelAnimationFrame(frame);
  }, [preferredFocus]);

  return (
    <Pressable
      accessibilityRole="button"
      disabled={disabled}
      focusable
      hasTVPreferredFocus={preferredFocus}
      ref={buttonRef}
      onBlur={() => setFocused(false)}
      onFocus={() => setFocused(true)}
      onPress={onPress}
      style={[
        styles.button,
        focused && styles.focused,
        disabled && styles.disabled,
      ]}>
      <Text style={styles.label}>{label}</Text>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  button: {
    minWidth: 170,
    paddingHorizontal: 20,
    paddingVertical: 11,
    borderRadius: 6,
    borderWidth: 2,
    borderColor: '#2b4255',
    backgroundColor: '#1769aa',
    alignItems: 'center',
  },
  focused: {
    borderColor: '#f4f7fa',
    backgroundColor: '#2388d8',
    transform: [{scale: 1.04}],
  },
  disabled: {
    opacity: 0.45,
  },
  label: {
    color: '#ffffff',
    fontSize: 16,
    fontWeight: '700',
  },
});
