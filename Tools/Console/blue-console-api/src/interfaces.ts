export interface ApiRequest {
  args: string[];

  requestInit(): RequestInit;
}

export interface ApiResponse<T> {
  success: boolean;
  response: T | ApiError;
}

export interface ApiError {
  errorCode: number;
  errorMessage: string;
}
