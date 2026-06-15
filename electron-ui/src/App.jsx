import React from 'react';
import TabDock from './components/TabDock';
import RetrieveTab from './tabs/RetrieveTab';
import CreateTab from './tabs/CreateTab';
import AnalyzeTab from './tabs/AnalyzeTab';

export default function App() {
  return (
    <TabDock
      tabs={[
        { name: 'Retrieve', content: <RetrieveTab /> },
        { name: 'Create', content: <CreateTab /> },
        { name: 'Analyze', content: <AnalyzeTab /> },
      ]}
    />
  );
}
