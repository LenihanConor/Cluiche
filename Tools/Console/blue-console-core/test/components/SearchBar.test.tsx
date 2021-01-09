import * as React from 'react';
import Enzyme, { shallow, mount } from 'enzyme';
import Adapter from 'enzyme-adapter-react-16';
import { SearchBar } from '../../src/components/SearchBar';
import { TextField, IconButton } from '@material-ui/core';

Enzyme.configure({ adapter: new Adapter() });

// eslint-disable-next-line @typescript-eslint/no-empty-function
const emptyFunc = (): void => {};
const mockEvent = { name: 'search-field', target: { value: 'testValue' } };

describe('SearchBar', () => {
  it('renders without crashing', () => {
    shallow(<SearchBar onSearch={emptyFunc} />);
  });

  it('calls onChange when value changes', () => {
    const onChange = jest.fn();

    const wrapper = shallow(<SearchBar onChange={onChange} onSearch={emptyFunc} />);
    wrapper.find(TextField).simulate('change', mockEvent);

    expect(onChange).toHaveBeenCalled();
  });

  it('calls onChange with expected value when value changes', () => {
    const onChange = jest.fn();

    const wrapper = shallow(<SearchBar onChange={onChange} onSearch={emptyFunc} />);
    wrapper.find(TextField).simulate('change', mockEvent);

    expect(onChange).toHaveBeenCalledWith(mockEvent.target.value);
  });

  it('calls onSearch with expected value when button clicked', () => {
    const onSearch = jest.fn();
    const expectedValue = 'test';

    const wrapper = mount(<SearchBar value={expectedValue} onSearch={onSearch} />);
    wrapper.find(IconButton).simulate('click', {});

    expect(onSearch).toHaveBeenCalledWith(expectedValue);
  });
});
