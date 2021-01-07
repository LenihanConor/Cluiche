import { Divider, List, ListSubheader, Paper } from "@material-ui/core";
import React, { ReactElement } from "react";

import NavLink from "../navigation/NavLink";
import type { Tool, ToolCategories } from "./ToolTypes";

function ToolNav({ entries }: ToolCategories): ReactElement {
  const row = (navItem: Tool): ReactElement => (
    <NavLink
      key={navItem.id}
      to={"/tools/" + navItem.id}
      primary={navItem.name}
    />
  );

  const category = (entry: any): ReactElement => {
    return (
      <List key={entry[0]} component="div">
        <ListSubheader>{entry[0]}</ListSubheader>
        {entry[1].map(row)}
      </List>
    );
  };

  return (
    <div>
      <Paper elevation={0}>
        <List aria-label="tool navigation">
          {Array.from(entries).map(category)}
        </List>
        <Divider />
      </Paper>
    </div>
  );
}

export default ToolNav;
