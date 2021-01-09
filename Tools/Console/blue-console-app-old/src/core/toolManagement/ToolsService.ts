import { Tool, ToolCategories } from "./ToolTypes";

class ToolsService {
  getByCategory(): Promise<ToolCategories> {
    return fetch("/plugins")
      .then((res) => res.json())
      .then((result) => {
        console.log(result);
        if (result.status === "ok") {
          const categories = new Map<string, Tool[]>();
          for (const value in result.body) {
            if ({}.hasOwnProperty.call(result.body, value)) {
              categories.set(value, result.body[value]);
            }
          }
          return Promise.resolve({ entries: categories } as ToolCategories);
        } else {
          return Promise.reject(result.error);
        }
      });
  }
}

export default ToolsService;
