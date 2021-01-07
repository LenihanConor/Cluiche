import Typography from "@material-ui/core/Typography";
import PropTypes from "prop-types";
import React from "react";

const Title: React.FC = (props: { children?: React.ReactNode }) => {
  return (
    <Typography component="h2" variant="h6" color="primary" gutterBottom>
      {props.children}
    </Typography>
  );
};

Title.propTypes = {
  children: PropTypes.node,
};

export default Title;
