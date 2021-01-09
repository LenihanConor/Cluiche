import { NavItemData } from './NavItemData.interface';

export interface NavGroupData {
  id: string;
  displayName: string;
  items: NavItemData[];
}
