import React from "react";

type State = {
  error: Error | null,
  errorInfo: React.ErrorInfo | null
}

class ToolContentErrorBoundary extends React.Component<Readonly<{}>, State> {
  constructor(props: Readonly<{}>) {
    super(props);
    this.state = { error: null, errorInfo: null };
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo) {
    this.setState({
      error: error,
      errorInfo: errorInfo
    })
  }

  render() {
    if (this.state.errorInfo) {
      return (
        <div>
          <h2>Encountered a problem with this tool.</h2>
          <details style={{ whiteSpace: 'pre-wrap' }}>
            {this.state.error && this.state.error.toString()}
            <br />
            {this.state.errorInfo.componentStack}
          </details>
        </div>
      )
    }

    return this.props.children;
  }
}

export default ToolContentErrorBoundary;
