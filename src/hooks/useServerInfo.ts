import {useCallback, useState} from 'react';
import {moonlightService} from '../services/moonlight';
import type {ServerInfo} from '../types/moonlight';

export const useServerInfo = () => {
  const [serverInfo, setServerInfo] = useState<ServerInfo | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const load = useCallback(async (host: string) => {
    setLoading(true);
    setError(null);

    try {
      const info = await moonlightService.getServerInfo(host);
      setServerInfo(info);
    } catch (reason) {
      setServerInfo(null);
      setError(reason instanceof Error ? reason.message : String(reason));
    } finally {
      setLoading(false);
    }
  }, []);

  return {serverInfo, error, loading, load};
};

