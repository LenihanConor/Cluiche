import { createRoot } from 'react-dom/client';
import { AppV2 } from './v2/AppV2';

const root = document.getElementById('root');
if (root) createRoot(root).render(<AppV2 />);
