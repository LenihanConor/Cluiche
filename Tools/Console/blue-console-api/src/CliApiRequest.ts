import { ApiRequest } from './interfaces';

export class CliApiRequest implements ApiRequest {
  args: string[];

  constructor(args: string[]) {
    this.args = args;
  }

  requestInit(): RequestInit {
    return {
      method: 'POST',
      body: JSON.stringify(this),
    };
  }
}
