import { Hidden } from '@material-ui/core';
import * as React from 'react';
import NavMenu from './NavMenu';

interface SideBarProps {
  width: number;
  drawerOpen: boolean;
  onDrawerToggle: () => void;
}

const SideBar: React.FC<SideBarProps> = (props: SideBarProps) => {
  return (
    <div>
      <Hidden smUp implementation="js">
        <NavMenu
          paperProps={{ style: { width: props.width } }}
          drawerProps={{ variant: 'temporary', open: props.drawerOpen, onClose: props.onDrawerToggle }}
        ></NavMenu>
      </Hidden>
      <Hidden xsDown implementation="css">
        <NavMenu paperProps={{ style: { width: props.width } }}></NavMenu>
      </Hidden>
    </div>
  );
};

export default SideBar;
