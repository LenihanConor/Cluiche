import * as React from 'react';

import NavGroup from './NavGroup';
import NavContext from '../../context/Nav.context';
import { NavGroupData } from '../../model/NavGroupData.interface';

const DynamicNavContent: React.FC = () => {
  const navContext = React.useContext(NavContext);

  const createNavGroup = (data: NavGroupData) => {
    return <NavGroup key={data.id} groupData={data}></NavGroup>;
  };

  return <React.Fragment>{navContext.navGroupData.map((data: NavGroupData) => createNavGroup(data))}</React.Fragment>;
};

export default DynamicNavContent;
