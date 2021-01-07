import "./index.css";

import React from "react";
import { render } from "react-dom";

import App from "./App/App";
import * as serviceWorker from "./serviceWorker";

render(<App />, document.getElementById("root"));

serviceWorker.unregister();
