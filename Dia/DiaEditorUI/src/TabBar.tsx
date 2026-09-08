import React, { useState } from 'react';
import { theme } from './theme';

export interface Tab {
  id: string;
  label: string;
  count?: number;
}

export interface TabBarProps {
  tabs: Tab[];
  activeTab: string;
  onTabChange: (id: string) => void;
}

const FONT_FAMILY = "'Segoe UI', system-ui, sans-serif";

export const TabBar: React.FC<TabBarProps> = ({ tabs, activeTab, onTabChange }) => {
  const [hoveredId, setHoveredId] = useState<string | null>(null);

  const containerStyle: React.CSSProperties = {
    display: 'flex',
    borderBottom: `1px solid ${theme.border}`,
    background: theme.bgPanel,
  };

  const getButtonStyle = (id: string): React.CSSProperties => {
    const isActive = id === activeTab;
    const isHovered = id === hoveredId;
    return {
      background: 'transparent',
      border: 'none',
      borderBottom: isActive ? `2px solid ${theme.accentHover}` : '2px solid transparent',
      padding: '6px 12px',
      fontSize: 12,
      cursor: 'pointer',
      fontFamily: FONT_FAMILY,
      color: isActive ? theme.text : isHovered ? theme.text : theme.textMuted,
    };
  };

  return (
    <div style={containerStyle} role="tablist" data-testid="tab-bar">
      {tabs.map((tab) => (
        <button
          key={tab.id}
          role="tab"
          aria-selected={tab.id === activeTab}
          data-tab-id={tab.id}
          onClick={() => onTabChange(tab.id)}
          onMouseEnter={() => setHoveredId(tab.id)}
          onMouseLeave={() => setHoveredId(null)}
          style={getButtonStyle(tab.id)}
        >
          {tab.count !== undefined ? `${tab.label} (${tab.count})` : tab.label}
        </button>
      ))}
    </div>
  );
};
