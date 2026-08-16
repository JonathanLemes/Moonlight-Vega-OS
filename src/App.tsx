import React from 'react';
import {SafeAreaView, StatusBar, StyleSheet} from 'react-native';
import {HostInfoScreen} from './screens/HostInfoScreen';

export const App = () => (
  <SafeAreaView style={styles.app}>
    <StatusBar barStyle="light-content" backgroundColor="#081018" />
    <HostInfoScreen />
  </SafeAreaView>
);

const styles = StyleSheet.create({
  app: {
    flex: 1,
    backgroundColor: '#081018',
  },
});

