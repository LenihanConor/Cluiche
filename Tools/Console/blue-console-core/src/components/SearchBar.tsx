import * as React from 'react';
import { FC, ChangeEvent, useState, useCallback } from 'react';
import { TextField, InputAdornment, IconButton } from '@material-ui/core';
import SearchIcon from '@material-ui/icons/Search';

export interface SearchBarProps {
  label?: string;
  value?: string;
  onChange?: (newValue: string | undefined) => void;
  onSearch: (searchTerm: string) => void;
}

export const SearchBar: FC<SearchBarProps> = (props: SearchBarProps) => {
  const [value, setValue] = useState(props.value);

  const handleRequestSearch = useCallback(() => {
    if (props.onSearch && value) {
      props.onSearch(value);
    }
  }, [props, value]);

  const handleChange = useCallback(
    (e: ChangeEvent<{ value: unknown }>) => {
      const newValue = e.target.value as string;
      setValue(newValue);
      if (props.onChange) {
        props.onChange(newValue);
      }
    },
    [props],
  );

  return (
    <TextField
      id="search-field"
      name="search-field"
      label={props.label || 'Search'}
      value={props.value}
      type="search"
      variant="outlined"
      onChange={handleChange}
      InputProps={{
        startAdornment: (
          <InputAdornment position="start">
            <IconButton onClick={handleRequestSearch}>
              <SearchIcon />
            </IconButton>
          </InputAdornment>
        ),
      }}
    />
  );
};
