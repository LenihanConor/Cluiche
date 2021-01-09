import { NavItemData } from '../model/NavItemData.interface';
import { NavGroupData } from '../model/NavGroupData.interface';
import { projectOverviewData } from '../model/ProjectOverviewData';

import { PluginData } from '../model/PluginData.interface';

class NavigationService {
  initializeFromPluginData(pluginData: PluginData[]): NavGroupData[] {
    const categories = this.getCategories(pluginData);
    return categories.map((category) => {
      return {
        id: category,
        displayName: category,
        items: this.getItemsForCategory(pluginData, category),
      };
    });
  }

  getProjectOverviewNavItemData(): NavItemData {
    return projectOverviewData;
  }

  getNavItemDataFromLocation(): NavItemData {
    // TODO: Actually determine appropriate NavItemData from window.location
    return projectOverviewData;
  }

  private getCategories(pluginData: PluginData[]): string[] {
    return Array.from(new Set(pluginData.map((element) => element.category)));
  }

  private getItemsForCategory(pluginData: PluginData[], category: string) {
    return pluginData
      .filter((element) => element.category === category)
      .map((element) => this.createNavItemData(element));
  }

  private createNavItemData(pluginData: PluginData): NavItemData {
    return {
      id: pluginData.id,
      displayName: pluginData.displayName,
      path: '/' + pluginData.id,
      iconName: pluginData.iconName,
    };
  }
}

export default NavigationService;
