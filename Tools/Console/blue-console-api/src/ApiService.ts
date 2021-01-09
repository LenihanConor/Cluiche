import { ApiError, ApiRequest, ApiResponse } from './interfaces';
import { CliApiRequest } from './CliApiRequest';
import fetch from 'cross-fetch';

export class ApiService {
  GENERAL_ERROR_CODE = 255;
  CLI_API_URL = '/api/cli';

  baseUrl: string;

  constructor(baseUrl: string) {
    this.baseUrl = baseUrl;
  }

  async cliRequest<ResponseType>(...args: string[]): Promise<ApiResponse<ResponseType>> {
    const request: ApiRequest = new CliApiRequest(args);
    return this.sendRequestInternal<ResponseType>(this.CLI_API_URL, request);
  }

  private async sendRequestInternal<T>(path: string, request: ApiRequest): Promise<ApiResponse<T>> {
    return new Promise((resolve) => {
      fetch(this.baseUrl + path, request.requestInit())
        .then((res) => {
          if (res.ok) {
            return res.json();
          } else {
            const apiError: ApiError = { errorCode: res.status, errorMessage: res.statusText };
            resolve({ success: false, response: apiError });
          }
        })
        .then((json) => {
          resolve({ success: true, response: json });
        })
        .catch((error) => {
          const apiError: ApiError = { errorCode: this.GENERAL_ERROR_CODE, errorMessage: error };
          resolve({ success: false, response: apiError });
        });
    });
  }
}
