import { ApiService } from '../src';
import { TestResponse } from './mocks/TestResponse';

describe('ApiService', () => {
  const DEFAULT_TEST_COMMAND = ['some', 'command'];

  let testObj: ApiService;

  beforeAll(() => {
    testObj = new ApiService('http://localhost');
  });

  describe('when sending a CLI valid request', () => {
    it('returns successful response', async () => {
      const res = await testObj.cliRequest<TestResponse>(...DEFAULT_TEST_COMMAND);
      expect(res.success).toBeTruthy();
    });

    it('returns expected response object type', async () => {
      const res = await testObj.cliRequest<TestResponse>(...DEFAULT_TEST_COMMAND);
      expectResponseTypeToBeTestResponse(res.response);
    });
  });

  describe('when sending a CLI request while server unreachable', () => {
    beforeAll(() => {
      testObj = new ApiService('http://fakehostname');
    });

    it('returns unsuccessful response', async () => {
      const res = await testObj.cliRequest<TestResponse>(...DEFAULT_TEST_COMMAND);
      expect(res.success).toBeFalsy();
    });

    it('returns expected response object type', async () => {
      const res = await testObj.cliRequest<TestResponse>(...DEFAULT_TEST_COMMAND);
      expectResponseTypeToBeApiError(res.response);
    });
  });

  function expectResponseTypeToBeTestResponse(obj: unknown) {
    expect(obj).toHaveProperty('data');
  }

  function expectResponseTypeToBeApiError(obj: unknown) {
    expect(obj).toHaveProperty('errorCode');
    expect(obj).toHaveProperty('errorMessage');
  }
});
