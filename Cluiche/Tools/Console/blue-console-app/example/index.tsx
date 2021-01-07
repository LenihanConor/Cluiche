import 'react-app-polyfill/ie11';
import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { ApiService } from '@ea-blue/console-api';
import { ConsoleApp } from '../.';

const App = () => {
  return <ConsoleApp consoleApiService={new ApiService('http://localhost:1400')} />;
};

ReactDOM.render(<App />, document.getElementById('root'));
