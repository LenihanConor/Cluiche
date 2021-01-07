export interface Tool {
  id: string;
  name: string;
  console_component_name: string;
  category: string;
  tags: string[];
}

export interface ToolCategories {
  entries: Map<string, Tool[]>;
}
