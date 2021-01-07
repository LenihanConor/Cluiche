import { cleanEnv, str } from 'envalid';

function validateEnv(): void {
  cleanEnv(process.env, {
    NODE_ENV: str(),
  });
}

export default validateEnv;
