import * as React from 'react';
import { Meta, Story } from '@storybook/react';
import { SearchBar } from '../.';

const meta: Meta = {
  title: 'SearchBar',
  component: SearchBar,
};

export default meta;

// eslint-disable-next-line @typescript-eslint/no-empty-function
const Template: Story = (args) => <SearchBar {...args} onSearch={(): void => {}} />;

// By passing using the Args format for exported stories, you can control the props for a component for reuse in a test
// https://storybook.js.org/docs/react/workflows/unit-testing
export const Default = Template.bind({});
export const WithValue = Template.bind({});

Default.args = {};
WithValue.args = { value: 'SomeValue' };
