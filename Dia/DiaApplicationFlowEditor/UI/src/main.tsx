import { createRoot } from 'react-dom/client';
import { injectThemeVars } from '@dia/editor-ui';
import { AppV2 } from './v2/AppV2';

injectThemeVars();

const root = document.getElementById('root');
if (root) createRoot(root).render(<AppV2 />);
