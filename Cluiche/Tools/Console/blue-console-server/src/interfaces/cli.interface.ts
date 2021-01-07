interface CliInterop {
  executeCommand(command: string): Promise<string>;
}

export default CliInterop;
