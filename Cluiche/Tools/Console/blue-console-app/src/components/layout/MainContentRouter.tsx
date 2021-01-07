import * as React from 'react';
import { Route, Switch } from 'react-router-dom';
import ProjectOverview from '../../pages/ProjectOverview';

const MainContentRouter: React.FC = () => {
  // Temp component
  const GenericContent = (props: { page: string }) => {
    return <div>[PLACEHOLDER] {props.page} content goes here</div>;
  };

  return (
    <Switch>
      <Route path="/" exact component={ProjectOverview} />
      <Route path="/package_manager">
        <GenericContent page="Package Manager" />
      </Route>
      <Route path="/game_data_editor">
        <GenericContent page="Game Data Editor" />
      </Route>
      <Route path="/debug_options">
        <GenericContent page="Debug Options" />
      </Route>
    </Switch>
  );
};

export default MainContentRouter;
