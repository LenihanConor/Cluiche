import 'process';

import CliInterop from 'interfaces/cli.interface';
import { exec } from 'child_process';

class BlueCliInterop implements CliInterop {
  public async executeCommand(command: string): Promise<string> {
    return new Promise((resolve, reject) => {
      exec(command, { cwd: '..' }, (error, stdout, stderr) => {
        if (error) {
          console.error(`Error executing '${command}': ${error} stderr: ${stderr}`);
          reject(error);
          return;
        }
        console.log(`${command} response: ${stdout}`);
        resolve(stdout);
      });
    });
  }
}

export default BlueCliInterop;
