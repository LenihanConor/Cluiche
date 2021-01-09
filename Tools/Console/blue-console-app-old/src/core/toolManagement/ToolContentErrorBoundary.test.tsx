import React from "react";
import { render } from "@testing-library/react";
import ToolContentErrorBoundary from "./ToolContentErrorBoundary";

const ComponentWithNoErrors = () => <div>ComponentWithNoErrors</div>;
class ComponentWithError extends React.Component {
    badEvaluation = () => {throw new Error("ComponentWithError threw")};

    render() {
        return <div>{this.badEvaluation()}</div>
    }
}

test("ToolContentErrorBoundary: with no error renders children", () => {
  const { getByText } = render(
    <ToolContentErrorBoundary>
      <ComponentWithNoErrors />
    </ToolContentErrorBoundary>
  );
  const linkElement = getByText(/ComponentWithNoErrors/i);
  expect(linkElement).toBeDefined();
});

test("ToolContentErrorBoundary: with error no boundary throws error", () => {
  expect(() => render(<ComponentWithError />)).toThrow("ComponentWithError threw");
});

test("ToolContentErrorBoundary: with error renders callstack", () => {
    const { getByText } = render(
      <ToolContentErrorBoundary>
        <ComponentWithError />
      </ToolContentErrorBoundary>
    );
    const linkElement = getByText(/in ComponentWithError \(at ToolContentErrorBoundary/i);
    expect(linkElement).toBeDefined();
});