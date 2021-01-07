import { Icon, ListItemIcon, ListItemText, MenuItem, Theme, withStyles } from '@material-ui/core';
import clsx from 'clsx';
import * as React from 'react';
import { useEffect } from 'react';
import { Link } from 'react-router-dom';
import NavContext from '../../context/Nav.context';
import { NavItemData } from '../../model/NavItemData.interface';

export interface NavItemProps {
  itemData: NavItemData;
  classes: {
    item: string;
    itemPrimary: string;
    itemActiveItem: string;
    itemIcon: string;
    itemExtra: string;
  };
}

const NavItem: React.FC<NavItemProps> = (props: NavItemProps) => {
  const [active, setActive] = React.useState(false);
  const navContext = React.useContext(NavContext);
  const { classes, itemData } = props;

  const handleClick = () => {
    if (navContext.onNavItemChanged) {
      navContext.onNavItemChanged(itemData);
    }
  };

  useEffect(() => {
    setActive(navContext.activeItem?.id === itemData.id);
  }, [navContext, itemData]);

  return (
    <MenuItem
      component={Link}
      to={itemData.path}
      className={clsx(classes.item, active && classes.itemActiveItem, classes.itemExtra)}
      onClick={handleClick}
    >
      <ListItemIcon className={classes.itemIcon}>
        <Icon>{itemData.iconName}</Icon>
      </ListItemIcon>
      <ListItemText classes={{ primary: classes.itemPrimary }}>{itemData.displayName}</ListItemText>
    </MenuItem>
  );
};

const styles = (theme: Theme) => ({
  item: {
    paddingTop: 1,
    paddingBottom: 1,
    color: 'rgba(255, 255, 255, 0.7)',
    '&:hover,&:focus': {
      backgroundColor: 'rgba(255, 255, 255, 0.08)',
    },
  },
  itemPrimary: {
    fontSize: 'inherit',
  },
  itemActiveItem: {
    color: '#4fc3f7',
  },
  itemIcon: {
    minWidth: 'auto',
    marginRight: theme.spacing(2),
    marginTop: 0,
  },
  itemExtra: {},
});

export default withStyles(styles)(NavItem);
