import { createRoot } from 'react-dom/client';
import { injectThemeVars } from '@dia/editor-ui';
import { App } from './App';

injectThemeVars();

const root = document.getElementById('root');
if (root) createRoot(root).render(<App />);
