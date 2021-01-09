import { NextFunction, Request, Response } from 'express';
import { ApiError, ApiRequest, ApiResponse } from '@ea-blue/console-api';
import CliInterop from 'interfaces/cli.interface';
import { ExecException } from 'child_process';

class CliController {
  private cli: CliInterop;

  constructor(cliInterop: CliInterop) {
    this.cli = cliInterop;
  }

  public exec = async (req: Request, res: Response, next: NextFunction): Promise<void> => {
    try {
      const request: ApiRequest = req.body;
      if (request.args && request.args.length > 0) {
        try {
          const result = await this.cli.executeCommand(request.args.join(' '));
          const response: ApiResponse<unknown> = { success: true, response: result || {} };
          res.send(response);
        } catch (error) {
          const execException = error as ExecException;
          const apiError: ApiError = { errorCode: execException.code, errorMessage: execException.message };
          const response: ApiResponse<unknown> = { success: false, response: apiError };
          res.status(502).send(response);
        }
      } else {
        const apiError: ApiError = { errorCode: 400, errorMessage: 'Request is invalid' };
        const response: ApiResponse<unknown> = { success: false, response: apiError };
        res.status(400).send(response);
      }
    } catch (error) {
      console.error(error);
      next(error);
    }
  };
}

export default CliController;
