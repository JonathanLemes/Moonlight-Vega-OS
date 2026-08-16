import React from 'react';
import {StatusBar, StyleSheet, View} from 'react-native';
import {HostInfoScreen} from './screens/HostInfoScreen';

export const App = () => (
  <View style={styles.app}>
    <StatusBar barStyle="light-content" backgroundColor="#081018" />
    <HostInfoScreen />
  </View>
);

const styles = StyleSheet.create({
  app: {
    flex: 1,
    backgroundColor: '#081018',
  },
});
