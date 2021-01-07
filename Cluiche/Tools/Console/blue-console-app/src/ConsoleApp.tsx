import * as React from 'react';
import { BrowserRouter as Router } from 'react-router-dom';
import { makeStyles, ThemeProvider } from '@material-ui/core/styles';
import CssBaseline from '@material-ui/core/CssBaseline';
import { ApiService } from '@ea-blue/console-api';

import theme from './styles/global.theme';
import Header from './components/layout/Header';
import Copyright from './components/layout/Copyright';
import SideBar from './components/navigation/SideBar';
import MainContentRouter from './components/layout/MainContentRouter';
import NavContext from './context/Nav.context';
import { NavItemData } from './model/NavItemData.interface';
import { PluginDataResponse } from './model/PluginDataResponse.interface';
import { NavGroupData } from './model/NavGroupData.interface';
import NavigationService from './service/NavigationService';

export interface ConsoleAppProps {
  consoleApiService?: ApiService;
}

export const ConsoleApp: React.FC<ConsoleAppProps> = (props: ConsoleAppProps) => {
  const consoleApiService = props.consoleApiService || new ApiService('');
  const navigationService = new NavigationService();
  const classes = useStyles();

  const [drawerOpen, setDrawerOpen] = React.useState(false);
  const [navGroupData, setNavGroupData] = React.useState<NavGroupData[]>([]);
  const [activeNavItem, setActiveNavItem] = React.useState(navigationService.getNavItemDataFromLocation());

  const handleDrawerToggle = () => {
    setDrawerOpen(!drawerOpen);
  };

  const handleNavItemChanged = (navItem: NavItemData) => {
    setActiveNavItem(navItem);
  };

  React.useEffect(() => {
    consoleApiService.cliRequest<PluginDataResponse>('blue', 'console', 'plugins', 'data').then((res) => {
      const groups = navigationService.initializeFromPluginData((res.response as PluginDataResponse).plugins);
      setNavGroupData(groups);
    });
  }, [consoleApiService, navigationService]);

  return (
    <Router>
      <ThemeProvider theme={theme}>
        <div className={classes.root}>
          <CssBaseline />
          <NavContext.Provider
            value={{
              navigationService: navigationService,
              navGroupData: navGroupData,
              activeItem: activeNavItem,
              onNavItemChanged: handleNavItemChanged,
            }}
          >
            <nav className={classes.drawer}>
              <SideBar width={drawerWidth} drawerOpen={drawerOpen} onDrawerToggle={handleDrawerToggle}></SideBar>
            </nav>
            <div className={classes.app}>
              <header className={classes.header}>
                <Header onDrawerToggle={handleDrawerToggle}></Header>
              </header>
              <main className={classes.main}>
                <MainContentRouter />
              </main>
              <footer className={classes.footer}>
                <Copyright></Copyright>
              </footer>
            </div>
          </NavContext.Provider>
        </div>
      </ThemeProvider>
    </Router>
  );
};

const drawerWidth = 256;
const useStyles = makeStyles({
  root: {
    display: 'flex',
    minHeight: '100vh',
  },
  drawer: {
    [theme.breakpoints.up('sm')]: {
      width: drawerWidth,
      flexShrink: 0,
    },
  },
  app: {
    flex: 1,
    display: 'flex',
    flexDirection: 'column',
  },
  header: {},
  main: {
    flex: 1,
    padding: theme.spacing(6, 4),
    background: '#eaeff1',
  },
  footer: {
    padding: theme.spacing(2),
    background: '#eaeff1',
  },
});
