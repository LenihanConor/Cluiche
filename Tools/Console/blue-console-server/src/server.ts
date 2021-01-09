import 'dotenv/config';
import App from './app';
import IndexRoute from './routes/index.route';
import PluginsRoute from './routes/plugins.route';
import CliRoute from './routes/cli.route';
import validateEnv from './utils/validateEnv';
import BlueCliInterop from './utils/blue-cli-interop';

validateEnv();

const cliInterop = new BlueCliInterop();
const app = new App([new IndexRoute(), new PluginsRoute(cliInterop), new CliRoute(cliInterop)]);

app.listen();
