import React, {useEffect, useRef, useState} from 'react';
import {ActivityIndicator, Pressable, StyleSheet, Text, View} from 'react-native';

interface FocusedButtonProps {
  label: string;
  disabled?: boolean;
  loading?: boolean;
  preferredFocus?: boolean;
  onPress: () => void;
}

export const FocusedButton = ({
  label,
  disabled = false,
  loading = false,
  preferredFocus = false,
  onPress,
}: FocusedButtonProps) => {
  const [focused, setFocused] = useState(false);
  const buttonRef = useRef<View & {requestTVFocus(): void}>(null);
  const isDisabled = disabled || loading;

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
      accessibilityState={{busy: loading, disabled: isDisabled}}
      disabled={isDisabled}
      focusable
      hasTVPreferredFocus={preferredFocus}
      ref={buttonRef}
      onBlur={() => setFocused(false)}
      onFocus={() => setFocused(true)}
      onPress={onPress}
      style={[
        styles.button,
        focused && styles.focused,
        isDisabled && styles.disabled,
        loading && styles.loading,
      ]}>
      {loading ? (
        <ActivityIndicator color="#ffffff" size="small" style={styles.spinner} />
      ) : null}
      <Text style={styles.label}>{label}</Text>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  button: {
    alignItems: 'center',
    backgroundColor: '#1769aa',
    borderColor: '#2b4255',
    borderRadius: 6,
    borderWidth: 3,
    flexDirection: 'row',
    justifyContent: 'center',
    minWidth: 170,
    paddingHorizontal: 20,
    paddingVertical: 10,
  },
  focused: {
    backgroundColor: '#2b96e0',
    borderColor: '#8fe3ff',
    elevation: 12,
    shadowColor: '#52c7f4',
    shadowOffset: {width: 0, height: 0},
    shadowOpacity: 1,
    shadowRadius: 12,
    transform: [{scale: 1.08}],
  },
  disabled: {
    opacity: 0.45,
  },
  loading: {
    opacity: 0.8,
  },
  spinner: {
    marginRight: 10,
  },
  label: {
    color: '#ffffff',
    fontSize: 16,
    fontWeight: '700',
  },
});
