import "./App.css";

import React from "react";
import { BrowserRouter, Route, Switch } from "react-router-dom";

import Console from "../core/dashboard/Console";

const App: React.FC = () => (
  <BrowserRouter>
    <Switch>
      <Route path="/" component={Console} />
    </Switch>
  </BrowserRouter>
);

export default App;
