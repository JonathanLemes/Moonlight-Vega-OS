import React, {useCallback, useEffect, useRef, useState} from 'react';
import {Animated, BackHandler, StyleSheet, Text, View} from 'react-native';
import {KeplerVideoSurfaceView} from '@amazon-devices/react-native-w3cmedia';
import {useMoonlightGamepad} from '../hooks/useMoonlightGamepad';
import {moonlightService} from '../services/moonlight';
import {VegaStreamPlayer} from '../services/vegaStreamPlayer';
import type {MoonlightApp} from '../types/moonlight';

interface Props {
  host: string;
  app: MoonlightApp;
  onExit: () => void;
}

const STATUS_VISIBLE_MS = 3000;
const STATUS_FADE_MS = 600;

export const StreamScreen = ({host, app, onExit}: Props) => {
  const playerRef = useRef<VegaStreamPlayer | null>(null);
  const stoppingRef = useRef(false);
  const [status, setStatus] = useState(`Starting ${app.name}…`);
  const [error, setError] = useState(false);
  const overlayOpacity = useRef(new Animated.Value(1)).current;
  const fadeTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  useMoonlightGamepad();

  // Surface every new status message immediately, then fade the overlay out
  // a few seconds later once things are quiet. Errors stay on screen since
  // they need to be read and acted on.
  useEffect(() => {
    if (fadeTimer.current) {
      clearTimeout(fadeTimer.current);
      fadeTimer.current = null;
    }
    overlayOpacity.stopAnimation();
    overlayOpacity.setValue(1);
    if (error) {
      return undefined;
    }
    fadeTimer.current = setTimeout(() => {
      Animated.timing(overlayOpacity, {
        toValue: 0,
        duration: STATUS_FADE_MS,
        useNativeDriver: true,
      }).start();
    }, STATUS_VISIBLE_MS);
    return () => {
      if (fadeTimer.current) {
        clearTimeout(fadeTimer.current);
        fadeTimer.current = null;
      }
    };
  }, [status, error, overlayOpacity]);

  const stop = useCallback(
    async (quitApp: boolean) => {
      if (stoppingRef.current) {
        return;
      }
      stoppingRef.current = true;
      setStatus(quitApp ? 'Stopping stream and app…' : 'Disconnecting…');
      try {
        await moonlightService.stopStream(host, quitApp);
      } catch (reason) {
        setStatus(reason instanceof Error ? reason.message : String(reason));
        setError(true);
      }
      await playerRef.current?.deinitialize().catch(() => undefined);
      playerRef.current = null;
      onExit();
    },
    [host, onExit],
  );

  useEffect(() => {
    const subscription = BackHandler.addEventListener('hardwareBackPress', () => {
      void stop(true);
      return true;
    });
    let cancelled = false;
    const start = async () => {
      const player = new VegaStreamPlayer((message, isError) => {
        if (!cancelled) {
          setStatus(message);
          setError(isError);
        }
      });
      playerRef.current = player;
      try {
        await player.initialize();
        if (!cancelled) {
          await moonlightService.startStream(host, app.id, {
            width: 1920,
            height: 1080,
            fps: 60,
            bitrateKbps: 20000,
            codec: 'h264',
          });
        }
      } catch (reason) {
        if (!cancelled) {
          setStatus(reason instanceof Error ? reason.message : String(reason));
          setError(true);
        }
      }
    };
    void start();
    return () => {
      cancelled = true;
      subscription.remove();
      if (!stoppingRef.current) {
        void moonlightService.stopStream(host, false).catch(() => undefined);
        void playerRef.current?.deinitialize().catch(() => undefined);
      }
    };
  }, [app.id, host, stop]);

  return (
    <View style={styles.page}>
      <KeplerVideoSurfaceView
        onSurfaceViewCreated={handle => playerRef.current?.setSurface(handle)}
        onSurfaceViewDestroyed={handle => playerRef.current?.clearSurface(handle)}
        scalingmode="fit"
        style={styles.video}
      />
      <Animated.View
        pointerEvents="none"
        style={[styles.overlay, {opacity: overlayOpacity}]}>
        <Text style={[styles.status, error && styles.error]}>{status}</Text>
        <Text style={styles.hint}>Press Back to stop {app.name}</Text>
      </Animated.View>
    </View>
  );
};

const styles = StyleSheet.create({
  page: {backgroundColor: '#000', flex: 1},
  video: {bottom: 0, left: 0, position: 'absolute', right: 0, top: 0},
  overlay: {
    backgroundColor: 'rgba(0,0,0,0.56)',
    borderRadius: 6,
    left: 20,
    paddingHorizontal: 14,
    paddingVertical: 9,
    position: 'absolute',
    top: 16,
  },
  status: {color: '#fff', fontSize: 15, fontWeight: '700'},
  error: {color: '#ff8f8f'},
  hint: {color: '#a8bac8', fontSize: 12, marginTop: 3},
});
