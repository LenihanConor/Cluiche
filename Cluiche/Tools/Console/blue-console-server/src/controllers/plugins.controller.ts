import { NextFunction, Request, Response } from 'express';
import CliInterop from 'interfaces/cli.interface';
import BlueCliInterop from '../utils/blue-cli-interop';

class PluginsController {
  private cli: CliInterop;

  constructor(cliInterop: CliInterop) {
    this.cli = cliInterop;
  }

  public index = async (_req: Request, res: Response, next: NextFunction): Promise<void> => {
    try {
      // TODO: Remove hardcoded example command
      const toolsCommand = 'blue tools group-by categories';
      const result = await this.cli.executeCommand(toolsCommand);
      res.send(result);
    } catch (error) {
      next(error);
    }
  };
}

export default PluginsController;
