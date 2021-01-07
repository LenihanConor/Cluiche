import React from 'react';
import * as ReactDOM from 'react-dom';
import { ConsoleApp } from '../src';

describe('ConsoleApp', () => {
  it('renders without crashing', () => {
    const div = document.createElement('div');
    ReactDOM.render(<ConsoleApp />, div);
    ReactDOM.unmountComponentAtNode(div);
  });
});
