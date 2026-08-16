import React, {useMemo, useState} from 'react';
import {StyleSheet, Text, TextInput, View} from 'react-native';
import {FocusedButton} from '../components/FocusedButton';
import {useServerInfo} from '../hooks/useServerInfo';
import {moonlightService} from '../services/moonlight';

export const HostInfoScreen = () => {
  const [host, setHost] = useState('');
  const {serverInfo, error, loading, load} = useServerInfo();
  const coreInfo = useMemo(() => moonlightService.getCoreInfo(), []);

  return (
    <View style={styles.page}>
      <View style={styles.header}>
        <Text style={styles.eyebrow}>MOONLIGHT FOR VEGA OS</Text>
        <Text style={styles.title}>Connect to Sunshine or Wolf</Text>
        <Text style={styles.subtitle}>
          Milestone 1 validates React Native → C++ TurboModule → GameStream.
        </Text>
      </View>

      <View style={styles.connectionRow}>
        <TextInput
          accessibilityLabel="Sunshine host address"
          autoCapitalize="none"
          autoCorrect={false}
          keyboardType="url"
          onChangeText={setHost}
          onSubmitEditing={() => load(host)}
          placeholder="Sunshine host name or IP address"
          placeholderTextColor="#7890a3"
          style={styles.input}
          value={host}
        />
        <FocusedButton
          disabled={loading || host.trim().length === 0}
          label={loading ? 'Connecting…' : 'Get server info'}
          onPress={() => load(host)}
        />
      </View>

      <View style={styles.card}>
        {error ? <Text style={styles.error}>{error}</Text> : null}
        {!error && !serverInfo ? (
          <Text style={styles.empty}>
            Enter a host to request its GameStream /serverinfo endpoint.
          </Text>
        ) : null}
        {serverInfo ? (
          <View>
            <Text style={styles.serverName}>
              {serverInfo.hostname || serverInfo.address}
            </Text>
            <InfoRow label="Address" value={`${serverInfo.address}:${serverInfo.port}`} />
            <InfoRow label="State" value={serverInfo.state || 'Unknown'} />
            <InfoRow label="App version" value={serverInfo.appVersion || 'Unknown'} />
            <InfoRow label="GameStream version" value={serverInfo.gsVersion || 'Unknown'} />
            <InfoRow label="Paired" value={serverInfo.paired ? 'Yes' : 'No'} />
            <InfoRow label="Current app" value={String(serverInfo.currentGame)} />
            <InfoRow
              label="Codec capability mask"
              value={String(serverInfo.serverCodecModeSupport)}
            />
          </View>
        ) : null}
      </View>

      <Text style={styles.footer}>
        Native core ready · moonlight-common-c linked · default {coreInfo.defaultWidth}×
        {coreInfo.defaultHeight}@{coreInfo.defaultFps}
      </Text>
    </View>
  );
};

const InfoRow = ({label, value}: {label: string; value: string}) => (
  <View style={styles.infoRow}>
    <Text style={styles.infoLabel}>{label}</Text>
    <Text style={styles.infoValue}>{value}</Text>
  </View>
);

const styles = StyleSheet.create({
  page: {
    flex: 1,
    paddingHorizontal: 64,
    paddingVertical: 36,
  },
  header: {
    marginBottom: 34,
  },
  eyebrow: {
    color: '#52c7f4',
    fontSize: 18,
    fontWeight: '800',
    letterSpacing: 2,
  },
  title: {
    color: '#ffffff',
    fontSize: 46,
    fontWeight: '800',
    marginTop: 8,
  },
  subtitle: {
    color: '#a8bac8',
    fontSize: 21,
    marginTop: 8,
  },
  connectionRow: {
    flexDirection: 'row',
    gap: 18,
    alignItems: 'center',
  },
  input: {
    flex: 1,
    height: 62,
    borderRadius: 8,
    borderWidth: 2,
    borderColor: '#2b4255',
    backgroundColor: '#111f2b',
    color: '#ffffff',
    fontSize: 22,
    paddingHorizontal: 20,
  },
  card: {
    flex: 1,
    marginTop: 28,
    borderRadius: 12,
    backgroundColor: '#101d28',
    padding: 28,
    justifyContent: 'center',
  },
  empty: {
    color: '#8197a8',
    fontSize: 22,
    textAlign: 'center',
  },
  error: {
    color: '#ff8f8f',
    fontSize: 22,
    textAlign: 'center',
  },
  serverName: {
    color: '#ffffff',
    fontSize: 32,
    fontWeight: '800',
    marginBottom: 16,
  },
  infoRow: {
    flexDirection: 'row',
    paddingVertical: 6,
  },
  infoLabel: {
    color: '#8fa5b5',
    width: 250,
    fontSize: 19,
  },
  infoValue: {
    color: '#ecf4f8',
    flex: 1,
    fontSize: 19,
  },
  footer: {
    color: '#647d8f',
    fontSize: 16,
    marginTop: 18,
  },
});

