import React from 'react';
import ReactDOM from 'react-dom/client';
import { injectThemeVars } from '@dia/editor-ui';
import AppInspector from './AppInspector';

injectThemeVars();

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <AppInspector />
  </React.StrictMode>
);
