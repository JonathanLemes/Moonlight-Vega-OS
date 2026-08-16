import React from 'react';
import renderer from 'react-test-renderer';
import {App} from '../src/App';

jest.mock('@moonlight-vega/core', () => ({
  MoonlightVegaCore: {
    getCoreInfo: () => ({
      moonlightCommonLinked: true,
      defaultWidth: 1920,
      defaultHeight: 1080,
      defaultFps: 60,
      launchQueryParameters: '',
    }),
  },
}));

it('renders the host connection screen', () => {
  renderer.act(() => {
    renderer.create(<App />);
  });
});
