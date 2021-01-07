import * as request from 'supertest';
import App from '../app';
import PluginsRoute from '../routes/plugins.route';

afterAll(async () => {
  await new Promise((resolve) => setTimeout(() => resolve(), 500));
});

describe('Testing Plugins Index', () => {
  describe('[GET] /plugins', () => {
    it('response statusCode 200', () => {
      const cliMock = { executeCommand: jest.fn() };
      const pluginsRoute = new PluginsRoute(cliMock);
      const app = new App([pluginsRoute]);

      return request(app.getServer()).get(`${pluginsRoute.path}`).expect(200);
    });
  });
});
