import React, {useEffect, useMemo, useRef, useState} from 'react';
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

const makePin = () =>
  __DEV__ ? '1234' : String(Math.floor(1000 + Math.random() * 9000));

export const HostInfoScreen = ({onLaunch}: Props) => {
  const [host, setHost] = useState('');
  const [discoveredHosts, setDiscoveredHosts] = useState<ServerInfo[]>([]);
  const [server, setServer] = useState<ServerInfo | null>(null);
  const [apps, setApps] = useState<MoonlightApp[]>([]);
  const [pin, setPin] = useState(makePin);
  const [loading, setLoading] = useState(false);
  const [connectingAddress, setConnectingAddress] = useState<string | null>(null);
  const [discovering, setDiscovering] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const coreInfo = useMemo(() => moonlightService.getCoreInfo(), []);

  const fail = (reason: unknown) => {
    if (reason instanceof Error) {
      setError(reason.message);
      return;
    }
    if (reason && typeof reason === 'object' && 'message' in reason) {
      setError(String(reason.message));
      return;
    }
    try {
      setError(typeof reason === 'string' ? reason : JSON.stringify(reason));
    } catch {
      setError(String(reason));
    }
  };

  const loadApps = async (address: string) => {
    const result = await moonlightService.getApps(address);
    setApps(result);
    if (result.length === 0) {
      setError('Wolf returned an empty application list.');
    }
  };

  useEffect(() => {
    let active = true;
    moonlightService
      .discoverHosts()
      .then(hosts => {
        if (active) {
          setDiscoveredHosts(hosts);
        }
      })
      .catch(fail)
      .finally(() => {
        if (active) {
          setDiscovering(false);
        }
      });
    return () => {
      active = false;
    };
  }, []);

  const discover = async () => {
    setDiscovering(true);
    setError(null);
    try {
      setDiscoveredHosts(await moonlightService.discoverHosts());
    } catch (reason) {
      fail(reason);
    } finally {
      setDiscovering(false);
    }
  };

  // The native GameStream calls below can block for a while (TLS handshake,
  // socket timeouts). Setting the loading state and then immediately awaiting
  // one of them back-to-back risks React never getting a chance to commit and
  // paint that state before the call ties up the thread — the button would
  // sit frozen instead of showing its spinner. Yielding to the next frame
  // first guarantees the loading UI is actually on screen before we start.
  const nextFrame = () =>
    new Promise<void>(resolve => requestAnimationFrame(() => resolve()));

  const connect = async (address: string) => {
    const selectedHost = address.trim();
    setHost(selectedHost);
    setConnectingAddress(selectedHost);
    setLoading(true);
    setError(null);
    setApps([]);
    await nextFrame();
    try {
      const info = await moonlightService.getServerInfo(selectedHost);
      setServer(info);
      try {
        // Wolf's public PairStatus can lag or reflect a different Moonlight
        // client. Authenticated HTTPS is the authoritative pairing check.
        await loadApps(selectedHost);
        setServer({...info, paired: true});
      } catch {
        setServer({...info, paired: false});
      }
    } catch (reason) {
      setServer(null);
      fail(reason);
    } finally {
      setLoading(false);
      setConnectingAddress(null);
    }
  };

  const pair = async () => {
    setLoading(true);
    setError(null);
    await nextFrame();
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

      {!server ? (
        <View style={styles.connectionRow}>
          <TextInput
            accessibilityLabel="Wolf host address"
            autoCapitalize="none"
            autoCorrect={false}
            keyboardType="url"
            onChangeText={setHost}
            onSubmitEditing={() => connect(host)}
            placeholder="Wolf or Sunshine host name / IP"
            placeholderTextColor="#7890a3"
            style={styles.input}
            value={host}
          />
          <FocusedButton
            disabled={host.trim().length === 0}
            label={loading ? 'Connecting…' : 'Connect'}
            loading={connectingAddress === host.trim()}
            onPress={() => connect(host)}
          />
        </View>
      ) : (
        <Text style={styles.connectedAddress}>
          {server.hostname || 'GameStream'} · {host}
        </Text>
      )}

      <View style={styles.card}>
        {error ? <Text style={styles.error}>{error}</Text> : null}
        {!server && discoveredHosts.length > 0 ? (
          <View style={styles.discovered}>
            <Text style={styles.discoveredTitle}>Servers on this network</Text>
            <View style={styles.hostButtons}>
              {discoveredHosts.map((item, index) => (
                <FocusedButton
                  key={`${item.uniqueId}-${item.address}`}
                  disabled={loading && connectingAddress !== item.address}
                  label={`${item.hostname || 'GameStream'} · ${item.address}`}
                  loading={connectingAddress === item.address}
                  onPress={() => connect(item.address)}
                  preferredFocus={index === 0}
                />
              ))}
            </View>
          </View>
        ) : null}
        {!server && discoveredHosts.length === 0 && !error ? (
          <View style={styles.discovered}>
            <Text style={styles.empty}>
              {discovering
                ? 'Searching the local network for Sunshine and Wolf…'
                : 'No server was found automatically. Enter its LAN address above.'}
            </Text>
            {!discovering ? (
              <FocusedButton
                label="Search again"
                onPress={discover}
                preferredFocus
              />
            ) : null}
          </View>
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
                label={loading ? 'Waiting for PIN…' : 'Start pairing'}
                loading={loading}
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
  const cardRef = useRef<View & {requestTVFocus(): void}>(null);
  useEffect(() => {
    if (!preferredFocus) {
      return;
    }
    const frame = requestAnimationFrame(() => {
      cardRef.current?.requestTVFocus();
    });
    return () => cancelAnimationFrame(frame);
  }, [preferredFocus]);
  return (
    <Pressable
      accessibilityLabel={`Launch ${app.name}`}
      accessibilityRole="button"
      focusable
      hasTVPreferredFocus={preferredFocus}
      ref={cardRef}
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
  page: {flex: 1, paddingHorizontal: 48, paddingVertical: 28},
  header: {marginBottom: 24},
  libraryHeader: {
    alignItems: 'center',
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 14,
  },
  eyebrow: {color: '#52c7f4', fontSize: 14, fontWeight: '800', letterSpacing: 1.5},
  title: {color: '#fff', fontSize: 32, fontWeight: '800', marginTop: 6},
  subtitle: {color: '#a8bac8', fontSize: 16, marginTop: 6},
  connectionRow: {alignItems: 'center', flexDirection: 'row', gap: 14},
  connectedAddress: {color: '#52c7f4', fontSize: 18, fontWeight: '700'},
  input: {
    backgroundColor: '#111f2b',
    borderColor: '#2b4255',
    borderRadius: 6,
    borderWidth: 2,
    color: '#fff',
    flex: 1,
    fontSize: 18,
    height: 48,
    includeFontPadding: false,
    paddingHorizontal: 16,
    paddingVertical: 0,
    textAlignVertical: 'center',
  },
  card: {
    backgroundColor: '#101d28',
    borderRadius: 10,
    flex: 1,
    justifyContent: 'center',
    marginTop: 20,
    padding: 20,
  },
  empty: {color: '#8197a8', fontSize: 17, textAlign: 'center'},
  discovered: {alignItems: 'center', gap: 16},
  discoveredTitle: {color: '#fff', fontSize: 20, fontWeight: '800'},
  hostButtons: {flexDirection: 'row', flexWrap: 'wrap', gap: 12},
  error: {color: '#ff8f8f', fontSize: 16, marginBottom: 12, textAlign: 'center'},
  pairing: {alignItems: 'center'},
  serverName: {color: '#52c7f4', fontSize: 17, fontWeight: '700'},
  pairTitle: {color: '#fff', fontSize: 24, fontWeight: '800', marginTop: 8},
  pairHelp: {color: '#a8bac8', fontSize: 15, marginTop: 6},
  pinRow: {alignItems: 'center', flexDirection: 'row', gap: 12, marginTop: 16},
  pin: {
    backgroundColor: '#071018',
    borderRadius: 6,
    color: '#fff',
    fontSize: 30,
    fontWeight: '900',
    letterSpacing: 8,
    paddingHorizontal: 18,
    paddingVertical: 8,
  },
  grid: {paddingBottom: 14},
  appCard: {
    alignItems: 'center',
    backgroundColor: '#101d28',
    borderColor: '#20394c',
    borderRadius: 10,
    borderWidth: 3,
    flex: 1,
    height: 150,
    justifyContent: 'center',
    margin: 7,
    maxWidth: '24%',
    padding: 12,
  },
  appCardFocused: {
    backgroundColor: '#17344a',
    borderColor: '#fff',
    transform: [{scale: 1.05}],
  },
  appIcon: {
    alignItems: 'center',
    backgroundColor: '#1769aa',
    borderRadius: 28,
    height: 56,
    justifyContent: 'center',
    width: 56,
  },
  appInitial: {color: '#fff', fontSize: 26, fontWeight: '900'},
  appName: {color: '#fff', fontSize: 15, fontWeight: '700', marginTop: 8, textAlign: 'center'},
  footer: {color: '#647d8f', fontSize: 13, marginTop: 10},
});
