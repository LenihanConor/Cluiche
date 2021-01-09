import {
  AppBar,
  Avatar,
  Grid,
  Hidden,
  IconButton,
  Link,
  Tabs,
  Theme,
  Toolbar,
  Typography,
  withStyles,
} from '@material-ui/core';
import * as React from 'react';
import clsx from 'clsx';
import MenuIcon from '@material-ui/icons/Menu';
import NavContext from '../../context/Nav.context';

interface HeaderProps {
  onDrawerToggle: () => void;
  classes: {
    menuButton: string;
    secondaryBar: string;
    iconButtonAvatar: string;
    link: string;
  };
}

const Header: React.FC<HeaderProps> = (props: HeaderProps) => {
  const { classes } = props;
  const navContext = React.useContext(NavContext);

  return (
    <React.Fragment>
      <AppBar color="primary" position="sticky" elevation={0}>
        <Toolbar>
          <Grid container spacing={1} alignItems="center">
            <Hidden smUp>
              <Grid item>
                <IconButton
                  color="inherit"
                  aria-label="open drawer"
                  onClick={props.onDrawerToggle}
                  className={clsx(classes.menuButton)}
                >
                  <MenuIcon />
                </IconButton>
              </Grid>
            </Hidden>
            <Grid item xs />
            <Grid item>
              <Link
                className={clsx(classes.link)}
                href="https://www.google.com/"
                target="_blank"
                rel="noreferrer"
                variant="body2"
              >
                Go to docs
              </Link>
            </Grid>
            <Grid item>
              <IconButton color="inherit" className={clsx(classes.iconButtonAvatar)}>
                <Avatar />
              </IconButton>
            </Grid>
          </Grid>
        </Toolbar>
      </AppBar>
      <AppBar component="div" className={classes.secondaryBar} color="primary" position="static" elevation={0}>
        <Toolbar>
          <Grid container alignItems="center" spacing={1}>
            <Grid item xs>
              <Typography color="inherit" variant="h5" component="h1">
                {navContext.activeItem?.displayName}
              </Typography>
            </Grid>
          </Grid>
        </Toolbar>
      </AppBar>
      <AppBar component="div" className={classes.secondaryBar} color="primary" position="static" elevation={0}>
        {/* TODO: Tabs*/}
        <Tabs value={0} textColor="inherit" />
      </AppBar>
    </React.Fragment>
  );
};

const lightColor = 'rgba(255, 255, 255, 0.7)';
const styles = (theme: Theme) => ({
  secondaryBar: {
    zIndex: 0,
  },
  menuButton: {
    marginLeft: -theme.spacing(1),
  },
  iconButtonAvatar: {
    padding: 4,
  },
  link: {
    textDecoration: 'none',
    color: lightColor,
    '&:hover': {
      color: theme.palette.common.white,
    },
  },
  button: {
    borderColor: lightColor,
  },
});

export default withStyles(styles)(Header);
