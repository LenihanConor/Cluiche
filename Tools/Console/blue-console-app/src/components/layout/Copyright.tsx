import { Typography } from '@material-ui/core';
import * as React from 'react';

const Copyright: React.FC = () => {
  return (
    <Typography variant="body2" color="textSecondary" align="center">
      {'Copyright © Electronic Arts ' + new Date().getFullYear()}
    </Typography>
  );
};

export default Copyright;
