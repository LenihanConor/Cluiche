import { Divider, ListItem, ListItemText, Theme, withStyles } from '@material-ui/core';
import clsx from 'clsx';
import * as React from 'react';
import { NavItemData } from '../../model/NavItemData.interface';
import { NavGroupData } from '../../model/NavGroupData.interface';
import NavItem from './NavItem';

export interface NavGroupProps {
  groupData: NavGroupData;
  classes: {
    categoryHeader: string;
    categoryHeaderPrimary: string;
    divider: string;
  };
}

const NavGroup: React.FC<NavGroupProps> = (props: NavGroupProps) => {
  const { classes, groupData } = props;

  const createNavItem = (data: NavItemData) => {
    return <NavItem key={data.id} itemData={data}></NavItem>;
  };

  return (
    <React.Fragment>
      <ListItem key={groupData.id} className={clsx(classes.categoryHeader)}>
        <ListItemText classes={{ primary: clsx(classes.categoryHeaderPrimary) }}>{groupData.displayName}</ListItemText>
      </ListItem>
      {groupData.items.map((itemData: NavItemData) => createNavItem(itemData))}
      <Divider className={clsx(classes.divider)} />
    </React.Fragment>
  );
};

const styles = (theme: Theme) => ({
  categoryHeader: {
    paddingTop: theme.spacing(2),
    paddingBottom: theme.spacing(2),
  },
  categoryHeaderPrimary: {
    color: theme.palette.common.white,
  },
  divider: {
    marginTop: theme.spacing(2),
  },
});

export default withStyles(styles)(NavGroup);
