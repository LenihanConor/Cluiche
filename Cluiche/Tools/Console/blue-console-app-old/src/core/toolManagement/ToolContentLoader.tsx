import React, { Suspense } from "react";
import { Tool, ToolCategories } from "./ToolTypes";
import { useParams } from "react-router";
import ToolContentErrorBoundary from "./ToolContentErrorBoundary";

const ToolContentLoader: React.FC<ToolCategories> = ({entries}: ToolCategories) => {
  const { toolId } = useParams();
  const component = mapIdToComponentName(toolId);
  const ToolComponent = React.lazy(
    () => import("../../.plugins/Tools/" + component)
  );

  function mapIdToComponentName(toolId: string) {
    let componentName = "";
    entries.forEach(function (category) {
      category.forEach(function (tool: Tool) {
        if (tool.id === toolId) {
          componentName = tool.console_component_name;
        }
      });
    });
    return componentName;
  }

  return (
    <div>
      {component && (
          <ToolContentErrorBoundary>
            <Suspense fallback="Loading plugin...">
                <ToolComponent />
            </Suspense>
          </ToolContentErrorBoundary>
      )}
    </div>
  );
};

export default ToolContentLoader;
