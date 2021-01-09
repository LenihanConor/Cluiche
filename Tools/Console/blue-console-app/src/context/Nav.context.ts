import * as React from 'react';
import { NavGroupData } from '../model/NavGroupData.interface';
import NavigationService from '../service/NavigationService';
import { NavItemData } from '../model/NavItemData.interface';

interface NavContextData {
  navigationService: NavigationService;
  navGroupData: NavGroupData[];
  activeItem?: NavItemData;
  onNavItemChanged?: (navItem: NavItemData) => void;
}

const navContext = React.createContext<NavContextData>({
  navigationService: new NavigationService(),
  navGroupData: [],
});

export default navContext;
