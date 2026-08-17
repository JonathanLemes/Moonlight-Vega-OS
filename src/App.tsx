import React, {useState} from 'react';
import {LogBox, StatusBar, StyleSheet, View} from 'react-native';
import {HostInfoScreen} from './screens/HostInfoScreen';
import {StreamScreen} from './screens/StreamScreen';
import type {MoonlightApp} from './types/moonlight';

if (__DEV__) {
  LogBox.ignoreAllLogs();
}

export const App = () => {
  const [stream, setStream] = useState<{host: string; app: MoonlightApp} | null>(null);
  return (
    <View style={styles.app}>
      <StatusBar barStyle="light-content" backgroundColor="#081018" />
      {stream ? (
        <StreamScreen
          app={stream.app}
          host={stream.host}
          onExit={() => setStream(null)}
        />
      ) : (
        <HostInfoScreen onLaunch={(host, app) => setStream({host, app})} />
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  app: {
    flex: 1,
    backgroundColor: '#081018',
  },
});
