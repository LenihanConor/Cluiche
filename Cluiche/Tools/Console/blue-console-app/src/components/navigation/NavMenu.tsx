import { Drawer, DrawerProps, List, ListItem, PaperProps, Theme, withStyles } from '@material-ui/core';
import clsx from 'clsx';
import * as React from 'react';
import DynamicNavContent from './DynamicNavContent';
import NavItem from './NavItem';
import NavContext from '../../context/Nav.context';

interface NavMenuProps {
  paperProps?: PaperProps;
  drawerProps?: DrawerProps;
  classes: {
    navHeader: string;
    item: string;
    itemCategory: string;
    itemPrimary: string;
    itemIcon: string;
  };
}

const NavMenu: React.FC<NavMenuProps> = (props: NavMenuProps) => {
  const { navigationService } = React.useContext(NavContext);
  const { classes } = props;

  return (
    <Drawer PaperProps={props.paperProps} variant="permanent" {...props.drawerProps}>
      <List disablePadding>
        <ListItem className={clsx(classes.navHeader, classes.item, classes.itemCategory)}>Console</ListItem>
        <NavItem
          classes={{ itemExtra: classes.itemCategory }}
          itemData={navigationService.getProjectOverviewNavItemData()}
        />
        <DynamicNavContent />
      </List>
    </Drawer>
  );
};

const styles = (theme: Theme) => ({
  navHeader: {
    fontSize: 24,
    color: theme.palette.common.white,
  },
  item: {
    paddingTop: 1,
    paddingBottom: 1,
    color: 'rgba(255, 255, 255, 0.7)',
    '&:hover,&:focus': {
      backgroundColor: 'rgba(255, 255, 255, 0.08)',
    },
  },
  itemCategory: {
    backgroundColor: '#232f3e',
    boxShadow: '0 -1px 0 #404854 inset',
    paddingTop: theme.spacing(2),
    paddingBottom: theme.spacing(2),
  },
  itemPrimary: {
    fontSize: 'inherit',
  },
  itemIcon: {
    minWidth: 'auto',
    marginRight: theme.spacing(2),
    marginTop: 0,
  },
});

export default withStyles(styles)(NavMenu);
