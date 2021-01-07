import { Router } from 'express';
import CliInterop from 'interfaces/cli.interface';
import PluginsController from '../controllers/plugins.controller';
import Route from '../interfaces/route.interface';

class PluginsRoute implements Route {
  public path = '/plugins';
  public router = Router();
  
  private pluginsController: PluginsController;

  constructor(cliInterop: CliInterop) {
    this.pluginsController = new PluginsController(cliInterop);
    this.initializeRelativeRoutes();
  }

  private initializeRelativeRoutes() {
    this.router.get('/', this.pluginsController.index);
  }
}

export default PluginsRoute;
