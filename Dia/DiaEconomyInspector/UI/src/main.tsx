import React from 'react';
import ReactDOM from 'react-dom/client';
import { injectThemeVars } from '@dia/editor-ui';
import EconomyInspector from './EconomyInspector';

injectThemeVars();

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <EconomyInspector />
  </React.StrictMode>
);
