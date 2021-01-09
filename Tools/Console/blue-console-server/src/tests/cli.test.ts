import { ApiRequest, ApiResponse } from '@ea-blue/console-api';
import { ExecException } from 'child_process';
import CliInterop from 'interfaces/cli.interface';
import Route from 'interfaces/route.interface';
import * as request from 'supertest';
import App from '../app';
import CliRoute from '../routes/cli.route';

afterAll(async () => {
  await new Promise((resolve) => setTimeout(() => resolve(), 500));
});

describe('Testing CLI exec', () => {
  let cliInteropMock: CliInterop;
  let route: Route;
  let app: App;

  beforeAll(() => {
    cliInteropMock = { executeCommand: jest.fn() };
    route = new CliRoute(cliInteropMock);
    app = new App([route]);
  });

  describe('[POST] /cli with valid args', () => {
    it('response statusCode 200', () => {
      const req: ApiRequest = { args: ['some', 'command'], requestInit: jest.fn() };

      return request(app.getServer()).post(`${route.path}`).send(req).expect(200);
    });

    it('response payload is expected type', () => {
      const req: ApiRequest = { args: ['some', 'command'], requestInit: jest.fn() };
      const expectedResponse: ApiResponse<unknown> = { success: true, response: {} };

      return request(app.getServer()).post(`${route.path}`).send(req).expect(expectedResponse);
    });
  });

  describe('[POST] /cli with invalid args', () => {
    it('response statusCode 400', () => {
      return request(app.getServer()).post(`${route.path}`).expect(400);
    });

    it('response payload is expected type', () => {
      const expectedResponse: ApiResponse<unknown> = {
        success: false,
        response: { errorCode: 400, errorMessage: 'Request is invalid' },
      };

      return request(app.getServer()).post(`${route.path}`).expect(expectedResponse);
    });
  });

  describe('[POST] /cli returns error', () => {
    const ERROR_CODE = -1;
    const ERROR_MESSAGE = 'faking execException';

    beforeAll(() => {
      executeCommandRejectsWithExecException(-1, 'faking execException');
    });

    it('response statusCode 502', () => {
      const req: ApiRequest = { args: ['some', 'command'], requestInit: jest.fn() };

      return request(app.getServer()).post(`${route.path}`).send(req).expect(502);
    });

    it('response is ApiError', () => {
      const req: ApiRequest = { args: ['some', 'command'], requestInit: jest.fn() };
      const expectedResponse: ApiResponse<unknown> = {
        success: false,
        response: { errorCode: ERROR_CODE, errorMessage: ERROR_MESSAGE },
      };

      return request(app.getServer()).post(`${route.path}`).send(req).expect(expectedResponse);
    });

    function executeCommandRejectsWithExecException(code: number, message: string) {
      const execException: ExecException = { name: 'test exception', code: code, message: message };
      cliInteropMock.executeCommand = jest.fn(() => Promise.reject(execException));
    }
  });
});
