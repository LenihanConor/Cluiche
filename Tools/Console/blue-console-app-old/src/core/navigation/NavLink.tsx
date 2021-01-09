import ListItem from "@material-ui/core/ListItem";
import ListItemIcon from "@material-ui/core/ListItemIcon";
import ListItemText from "@material-ui/core/ListItemText";
import { Omit } from "@material-ui/types";
import React, { ReactElement } from "react";
import {
  Link as RouterLink,
  LinkProps as RouterLinkProps,
} from "react-router-dom";

interface NavItemLinkProps {
  icon?: React.ReactElement;
  primary: string;
  to: string;
}

function NavLink(props: NavItemLinkProps): ReactElement {
  const { icon, primary, to } = props;

  function linkElement(itemProps: any, ref: any): ReactElement {
    return <RouterLink to={to} ref={ref} {...itemProps} />;
  }

  function linkRef(): any {
    return React.forwardRef<RouterLink, Omit<RouterLinkProps, "to">>(
      linkElement
    );
  }

  const link = React.useMemo(linkRef, [to]);

  return (
    <li>
      <ListItem button component={link}>
        {icon ? <ListItemIcon>{icon}</ListItemIcon> : null}
        <ListItemText primary={primary} />
      </ListItem>
    </li>
  );
}

export default NavLink;
