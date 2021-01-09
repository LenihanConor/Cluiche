import { NextFunction, Request, Response } from 'express';

class IndexController {
  public index = (_req: Request, res: Response, next: NextFunction): void => {
    try {
      res.status(200).send();
    } catch (error) {
      next(error);
    }
  };
}

export default IndexController;
