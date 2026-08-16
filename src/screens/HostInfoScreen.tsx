import React, {useMemo, useState} from 'react';
import {
  FlatList,
  Pressable,
  StyleSheet,
  Text,
  TextInput,
  View,
} from 'react-native';
import {FocusedButton} from '../components/FocusedButton';
import {moonlightService} from '../services/moonlight';
import type {MoonlightApp, ServerInfo} from '../types/moonlight';

interface Props {
  onLaunch: (host: string, app: MoonlightApp) => void;
}

const makePin = () => String(Math.floor(1000 + Math.random() * 9000));

export const HostInfoScreen = ({onLaunch}: Props) => {
  const [host, setHost] = useState('');
  const [server, setServer] = useState<ServerInfo | null>(null);
  const [apps, setApps] = useState<MoonlightApp[]>([]);
  const [pin, setPin] = useState(makePin);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const coreInfo = useMemo(() => moonlightService.getCoreInfo(), []);

  const fail = (reason: unknown) => {
    setError(reason instanceof Error ? reason.message : String(reason));
  };

  const loadApps = async (address: string) => {
    const result = await moonlightService.getApps(address);
    setApps(result);
    if (result.length === 0) {
      setError('Wolf returned an empty application list.');
    }
  };

  const connect = async () => {
    setLoading(true);
    setError(null);
    setApps([]);
    try {
      const info = await moonlightService.getServerInfo(host);
      setServer(info);
      if (info.paired) {
        try {
          await loadApps(host);
        } catch {
          // PairStatus can reflect another Moonlight client. HTTPS proves whether
          // this app's persisted client certificate is paired.
          setServer({...info, paired: false});
        }
      }
    } catch (reason) {
      setServer(null);
      fail(reason);
    } finally {
      setLoading(false);
    }
  };

  const pair = async () => {
    setLoading(true);
    setError(null);
    try {
      await moonlightService.pair(host, pin);
      const info = await moonlightService.getServerInfo(host);
      setServer(info);
      await loadApps(host);
    } catch (reason) {
      fail(reason);
    } finally {
      setLoading(false);
    }
  };

  if (apps.length > 0) {
    return (
      <View style={styles.page}>
        <View style={styles.libraryHeader}>
          <View>
            <Text style={styles.eyebrow}>CONNECTED TO {server?.hostname || host}</Text>
            <Text style={styles.title}>Choose an app</Text>
          </View>
          <FocusedButton
            label="Change server"
            onPress={() => {
              setApps([]);
              setServer(null);
            }}
          />
        </View>
        <FlatList
          contentContainerStyle={styles.grid}
          data={apps}
          keyExtractor={item => String(item.id)}
          numColumns={4}
          renderItem={({item, index}) => (
            <AppCard
              app={item}
              preferredFocus={index === 0}
              onPress={() => onLaunch(host.trim(), item)}
            />
          )}
        />
        <Text style={styles.footer}>
          Fire TV remote and Bluetooth gamepads navigate this screen. Streaming
          defaults to H.264 · 1080p60 · 20 Mbps.
        </Text>
      </View>
    );
  }

  return (
    <View style={styles.page}>
      <View style={styles.header}>
        <Text style={styles.eyebrow}>MOONLIGHT FOR VEGA OS</Text>
        <Text style={styles.title}>Connect to Sunshine or Wolf</Text>
        <Text style={styles.subtitle}>
          Native GameStream, hardware video playback, Opus audio, and controller input.
        </Text>
      </View>

      <View style={styles.connectionRow}>
        <TextInput
          accessibilityLabel="Wolf host address"
          autoCapitalize="none"
          autoCorrect={false}
          keyboardType="url"
          onChangeText={setHost}
          onSubmitEditing={connect}
          placeholder="Wolf or Sunshine host name / IP"
          placeholderTextColor="#7890a3"
          style={styles.input}
          value={host}
        />
        <FocusedButton
          disabled={loading || host.trim().length === 0}
          label={loading ? 'Connecting…' : 'Connect'}
          onPress={connect}
          preferredFocus
        />
      </View>

      <View style={styles.card}>
        {error ? <Text style={styles.error}>{error}</Text> : null}
        {!server && !error ? (
          <Text style={styles.empty}>
            Enter the LAN address of the computer running Wolf or Sunshine.
          </Text>
        ) : null}
        {server && !server.paired ? (
          <View style={styles.pairing}>
            <Text style={styles.serverName}>{server.hostname || host}</Text>
            <Text style={styles.pairTitle}>Pair this Fire TV</Text>
            <Text style={styles.pairHelp}>
              Start pairing, then enter this PIN in the Wolf/Sunshine pairing page.
            </Text>
            <View style={styles.pinRow}>
              <Text style={styles.pin}>{pin}</Text>
              <FocusedButton
                disabled={loading}
                label={loading ? 'Waiting for PIN…' : 'Start pairing'}
                onPress={pair}
                preferredFocus
              />
              <FocusedButton label="New PIN" onPress={() => setPin(makePin())} />
            </View>
          </View>
        ) : null}
        {server?.paired && loading ? (
          <Text style={styles.empty}>Loading Wolf integrations…</Text>
        ) : null}
      </View>

      <Text style={styles.footer}>
        moonlight-common-c linked · default {coreInfo.defaultWidth}×
        {coreInfo.defaultHeight}@{coreInfo.defaultFps}
      </Text>
    </View>
  );
};

const AppCard = ({
  app,
  preferredFocus,
  onPress,
}: {
  app: MoonlightApp;
  preferredFocus: boolean;
  onPress: () => void;
}) => {
  const [focused, setFocused] = useState(false);
  return (
    <Pressable
      accessibilityLabel={`Launch ${app.name}`}
      accessibilityRole="button"
      focusable
      hasTVPreferredFocus={preferredFocus}
      onBlur={() => setFocused(false)}
      onFocus={() => setFocused(true)}
      onPress={onPress}
      style={[styles.appCard, focused && styles.appCardFocused]}>
      <View style={styles.appIcon}>
        <Text style={styles.appInitial}>{app.name.slice(0, 1).toUpperCase()}</Text>
      </View>
      <Text numberOfLines={2} style={styles.appName}>
        {app.name}
      </Text>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  page: {flex: 1, paddingHorizontal: 64, paddingVertical: 36},
  header: {marginBottom: 34},
  libraryHeader: {
    alignItems: 'center',
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 18,
  },
  eyebrow: {color: '#52c7f4', fontSize: 18, fontWeight: '800', letterSpacing: 2},
  title: {color: '#fff', fontSize: 46, fontWeight: '800', marginTop: 8},
  subtitle: {color: '#a8bac8', fontSize: 21, marginTop: 8},
  connectionRow: {alignItems: 'center', flexDirection: 'row', gap: 18},
  input: {
    backgroundColor: '#111f2b',
    borderColor: '#2b4255',
    borderRadius: 8,
    borderWidth: 2,
    color: '#fff',
    flex: 1,
    fontSize: 22,
    height: 62,
    paddingHorizontal: 20,
  },
  card: {
    backgroundColor: '#101d28',
    borderRadius: 12,
    flex: 1,
    justifyContent: 'center',
    marginTop: 28,
    padding: 28,
  },
  empty: {color: '#8197a8', fontSize: 22, textAlign: 'center'},
  error: {color: '#ff8f8f', fontSize: 20, marginBottom: 18, textAlign: 'center'},
  pairing: {alignItems: 'center'},
  serverName: {color: '#52c7f4', fontSize: 22, fontWeight: '700'},
  pairTitle: {color: '#fff', fontSize: 34, fontWeight: '800', marginTop: 10},
  pairHelp: {color: '#a8bac8', fontSize: 19, marginTop: 8},
  pinRow: {alignItems: 'center', flexDirection: 'row', gap: 18, marginTop: 24},
  pin: {
    backgroundColor: '#071018',
    borderRadius: 8,
    color: '#fff',
    fontSize: 42,
    fontWeight: '900',
    letterSpacing: 12,
    paddingHorizontal: 24,
    paddingVertical: 10,
  },
  grid: {paddingBottom: 20},
  appCard: {
    alignItems: 'center',
    backgroundColor: '#101d28',
    borderColor: '#20394c',
    borderRadius: 12,
    borderWidth: 4,
    flex: 1,
    height: 210,
    justifyContent: 'center',
    margin: 10,
    maxWidth: '24%',
    padding: 16,
  },
  appCardFocused: {
    backgroundColor: '#17344a',
    borderColor: '#fff',
    transform: [{scale: 1.05}],
  },
  appIcon: {
    alignItems: 'center',
    backgroundColor: '#1769aa',
    borderRadius: 42,
    height: 84,
    justifyContent: 'center',
    width: 84,
  },
  appInitial: {color: '#fff', fontSize: 42, fontWeight: '900'},
  appName: {color: '#fff', fontSize: 20, fontWeight: '700', marginTop: 14, textAlign: 'center'},
  footer: {color: '#647d8f', fontSize: 16, marginTop: 14},
});
