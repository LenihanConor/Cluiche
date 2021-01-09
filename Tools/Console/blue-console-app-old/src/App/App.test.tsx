import React from "react";
import { render } from "@testing-library/react";
import App from "./App";

test("App: renders copyright text", () => {
  const { getByText } = render(<App />);
  const linkElement = getByText(/Copyright /i);
  expect(linkElement).toBeDefined();
});
