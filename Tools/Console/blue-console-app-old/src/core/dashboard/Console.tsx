import AppBar from "@material-ui/core/AppBar";
import Badge from "@material-ui/core/Badge";
import Box from "@material-ui/core/Box";
import Container from "@material-ui/core/Container";
import CssBaseline from "@material-ui/core/CssBaseline";
import Divider from "@material-ui/core/Divider";
import Drawer from "@material-ui/core/Drawer";
import Grid from "@material-ui/core/Grid";
import IconButton from "@material-ui/core/IconButton";
import { makeStyles } from "@material-ui/core/styles";
import Toolbar from "@material-ui/core/Toolbar";
import Typography from "@material-ui/core/Typography";
import ChevronLeftIcon from "@material-ui/icons/ChevronLeft";
import MenuIcon from "@material-ui/icons/Menu";
import NotificationsIcon from "@material-ui/icons/Notifications";
import clsx from "clsx";
import React, { ReactElement, Suspense, useEffect, useState } from "react";
import { Route, Switch } from "react-router-dom";

import Copyright from './Copyright'
import MainNav from "../navigation/MainNav";
import ToolNav from "../toolManagement/ToolNav";
import Tools from "../toolManagement/Tools";
import { Tool, ToolCategories } from "../toolManagement/ToolTypes";
import ToolsService from "../toolManagement/ToolsService";
import Home from "../home/Home";
import ToolContentLoader from "../toolManagement/ToolContentLoader";

const drawerWidth = 180;

const Console = (): ReactElement => {
  const [tools, setTools] = useState({
    entries: new Map<string, Tool[]>(),
  } as ToolCategories);
  useEffect(() => {
    const toolsService = new ToolsService();
    toolsService.getByCategory()
      .then(tools => setTools(tools))
  }, []);

  const useStyles = makeStyles((theme) => ({
    appBar: {
      transition: theme.transitions.create(["width", "margin"], {
        duration: theme.transitions.duration.leavingScreen,
        easing: theme.transitions.easing.sharp,
      }),
      zIndex: theme.zIndex.drawer + 1,
    },
    appBarShift: {
      marginLeft: drawerWidth,
      transition: theme.transitions.create(["width", "margin"], {
        duration: theme.transitions.duration.enteringScreen,
        easing: theme.transitions.easing.sharp,
      }),
      width: `calc(100% - ${drawerWidth}px)`,
    },
    appBarSpacer: theme.mixins.toolbar,
    container: {
      paddingBottom: theme.spacing(4),
      paddingTop: theme.spacing(4),
    },
    content: {
      flexGrow: 1,
      height: "100vh",
      overflow: "auto",
    },
    drawerPaper: {
      position: "relative",
      transition: theme.transitions.create("width", {
        duration: theme.transitions.duration.enteringScreen,
        easing: theme.transitions.easing.sharp,
      }),
      whiteSpace: "nowrap",
      width: drawerWidth,
    },
    drawerPaperClose: {
      overflowX: "hidden",
      transition: theme.transitions.create("width", {
        duration: theme.transitions.duration.leavingScreen,
        easing: theme.transitions.easing.sharp,
      }),
      width: theme.spacing(7),
      [theme.breakpoints.up("sm")]: {
        width: theme.spacing(9),
      },
    },
    fixedHeight: {
      height: 240,
    },
    menuButton: {
      marginRight: 36,
    },
    menuButtonHidden: {
      display: "none",
    },
    paper: {
      display: "flex",
      flexDirection: "column",
      overflow: "auto",
      padding: theme.spacing(2),
    },
    root: {
      display: "flex",
    },
    title: {
      flexGrow: 1,
    },
    toolNav: {
      width: 180,
    },
    toolbar: {
      paddingRight: 24, // keep right padding when drawer closed
    },
    toolbarIcon: {
      alignItems: "center",
      display: "flex",
      justifyContent: "flex-end",
      padding: "0 8px",
      ...theme.mixins.toolbar,
    },
  }));

  const classes = useStyles();
  const [open, setOpen] = React.useState(true);
  const handleDrawerOpen = (): void => {
    setOpen(true);
  };
  const handleDrawerClose = (): void => {
    setOpen(false);
  };

  return (
    <div className={classes.root}>
      <CssBaseline />
      <AppBar
        position="absolute"
        className={clsx(classes.appBar, open && classes.appBarShift)}
      >
        <Toolbar className={classes.toolbar}>
          <IconButton
            edge="start"
            color="inherit"
            aria-label="open drawer"
            onClick={handleDrawerOpen}
            className={clsx(
              classes.menuButton,
              open && classes.menuButtonHidden
            )}
          >
            <MenuIcon />
          </IconButton>
          <Typography
            component="h1"
            variant="h6"
            color="inherit"
            align="left"
            noWrap
            className={classes.title}
          >
            Blue Console
          </Typography>
          <IconButton color="inherit">
            <Badge badgeContent={4} color="secondary">
              <NotificationsIcon />
            </Badge>
          </IconButton>
        </Toolbar>
      </AppBar>
      <Drawer
        variant="permanent"
        classes={{
          paper: clsx(classes.drawerPaper, !open && classes.drawerPaperClose),
        }}
        open={open}
      >
        <div className={classes.toolbarIcon}>
          <IconButton onClick={handleDrawerClose}>
            <ChevronLeftIcon />
          </IconButton>
        </div>
        <Divider />
        <MainNav />
      </Drawer>
      <main className={classes.content}>
        <div className={classes.appBarSpacer} />
        <Container maxWidth="lg" className={classes.container}>
          <Grid container>
            <Grid item className={classes.toolNav}>
              <Suspense fallback="<div>Loading...<div>">
                <ToolNav entries={tools.entries} />
              </Suspense>
            </Grid>
            <Grid item xs>
              <Switch>
                <Route exact path="/home">
                  <Home />
                </Route>
                <Route
                  path="/tools/:toolId"
                  render={() =>
                    <ToolContentLoader entries={tools.entries}/>
                  }
                >
                </Route>
                <Route exact path="/tools">
                    <Tools />
                </Route>
              </Switch>
            </Grid>
          </Grid>
          <Box pt={4}>
            <Copyright />
          </Box>
        </Container>
      </main>
    </div>
  );
};

export default Console;
