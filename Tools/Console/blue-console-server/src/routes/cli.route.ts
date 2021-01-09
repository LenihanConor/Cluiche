import { Router } from 'express';
import CliInterop from 'interfaces/cli.interface';
import CliController from '../controllers/cli.controller';
import Route from '../interfaces/route.interface';

class CliRoute implements Route {
  public path = '/api/cli';
  public router = Router();

  private cliController: CliController;

  constructor(cliInterop: CliInterop) {
    this.cliController = new CliController(cliInterop);
    this.initializeRelativeRoutes();
  }

  private initializeRelativeRoutes() {
    this.router.post('/', this.cliController.exec);
  }
}

export default CliRoute;
