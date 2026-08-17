import React from 'react';
import renderer from 'react-test-renderer';
import {App} from '../src/App';

jest.mock('@amazon-devices/react-native-w3cmedia', () => {
  const ReactForMock = jest.requireActual<typeof React>('react');
  const {View} = jest.requireActual('react-native') as typeof import('react-native');
  return {
    KeplerVideoSurfaceView: (props: object) =>
      ReactForMock.createElement(View, props),
    MediaPlayer: class {},
    MediaSource: class {},
  };
});

jest.mock('@amazon-devices/react-native-kepler', () => ({
  useGamepadEventHandler: () => undefined,
}));

jest.mock('@moonlight-vega/core', () => ({
  MoonlightVegaCore: {
    getCoreInfo: () => ({
      moonlightCommonLinked: true,
      defaultWidth: 1920,
      defaultHeight: 1080,
      defaultFps: 60,
      launchQueryParameters: '',
    }),
    discoverHosts: async () => ({hosts: []}),
  },
}));

it('renders the host connection screen', async () => {
  let tree: renderer.ReactTestRenderer | undefined;
  await renderer.act(async () => {
    tree = renderer.create(<App />);
  });
  expect(
    tree?.root.findByProps({accessibilityLabel: 'Wolf host address'}),
  ).toBeDefined();
  renderer.act(() => tree?.unmount());
});
