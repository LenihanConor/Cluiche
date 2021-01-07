import { rest } from 'msw';
import { TestResponse } from './TestResponse';

export const handlers = [
  rest.post('http://localhost/api/cli', async (req, res, ctx) => {
    const responseData: TestResponse = { data: 'test' };
    return res(ctx.json(responseData));
  }),
];
