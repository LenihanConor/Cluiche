import 'react-app-polyfill/ie11';
import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { SearchBar } from '../.';

const App = () => {
  return (
    <div>
      <SearchBar onSearchClicked={(searchTerm) => console.log(searchTerm)} />
    </div>
  );
};

ReactDOM.render(<App />, document.getElementById('root'));
