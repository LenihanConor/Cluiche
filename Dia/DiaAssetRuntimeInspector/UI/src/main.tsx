import React from 'react';
import ReactDOM from 'react-dom/client';
import { injectThemeVars } from '@dia/editor-ui';
import App from './App';

injectThemeVars();

ReactDOM.createRoot(document.getElementById('root')!).render(
    <React.StrictMode>
        <App />
    </React.StrictMode>
);
