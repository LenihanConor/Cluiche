import Link from "@material-ui/core/Link";
import { makeStyles, Theme } from "@material-ui/core/styles";
import Table from "@material-ui/core/Table";
import TableBody from "@material-ui/core/TableBody";
import TableCell from "@material-ui/core/TableCell";
import TableHead from "@material-ui/core/TableHead";
import TableRow from "@material-ui/core/TableRow";
import React, { ReactElement } from "react";

import Title from "../../Title";

interface ToolRow {
  id: number;
  name: string;
}

function createData(id: number, name: string): ToolRow {
  return { id, name };
}

const rows = [createData(0, "Tool 1"), createData(1, "Tool 2")];

function preventDefault(event: { preventDefault: () => void }): void {
  event.preventDefault();
}

function Tools(): ReactElement {
  function row(toolRow: ToolRow): ReactElement {
    return (
      <TableRow key={toolRow.id}>
        <TableCell>{toolRow.name}</TableCell>
      </TableRow>
    );
  }

  // eslint-disable-next-line @typescript-eslint/explicit-function-return-type
  function linkStyle(theme: Theme) {
    return {
      seeMore: {
        marginTop: theme.spacing(3),
      },
    };
  }

  const useStyles = makeStyles(linkStyle);

  const classes = useStyles();
  return (
    <React.Fragment>
      <Title>Tools</Title>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Name</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>{rows.map(row)}</TableBody>
      </Table>
      <div className={classes.seeMore}>
        <Link color="primary" href="#" onClick={preventDefault}>
          See more tools
        </Link>
      </div>
    </React.Fragment>
  );
}

export default Tools;
