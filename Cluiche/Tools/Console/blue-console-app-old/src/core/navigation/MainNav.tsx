import Divider from "@material-ui/core/Divider";
import List from "@material-ui/core/List";
import Paper from "@material-ui/core/Paper";
import { makeStyles } from "@material-ui/core/styles";
import BuildIcon from "@material-ui/icons/Build";
import HomeIcon from "@material-ui/icons/Home";
import React, { ReactElement } from "react";

import NavLink from "./NavLink";

const useStyles = makeStyles({
  root: {
    width: 179,
  },
});

const MainNav = function (): ReactElement {
  const classes = useStyles();

  return (
    <div className={classes.root}>
      <Paper elevation={0}>
        <List aria-label="main navigation">
          <NavLink to="/home" primary="Home" icon={<HomeIcon />} />
          <NavLink to="/tools" primary="Tools" icon={<BuildIcon />} />
        </List>
        <Divider />
      </Paper>
    </div>
  );
};

export default MainNav;
