import React, { useState } from 'react';
import { HConnector, VConnector, Dock } from '../components/TabLayout';
import DockPanel from '../components/DockPanel';
import Retrieve from '../panels/Retrieve';
import PreviewDataFileList from '../panels/PreviewDataFileList';
import PreviewChart from '../panels/PreviewChart';
import TablePreview from '../panels/TablePreview';
import Simulate from '../panels/Simulate';


export default function RetrieveTab() {
  const [hx, setHx] = useState(0.25);
  const [vy, setVy] = useState(0.30);

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      {/* Left: Retrieve */}
      <Dock x={0} y={0} w={hx} h={1}>
        <DockPanel dockId="r-left" panels={[
          { name: 'Retrieve', component: <Retrieve /> },
          { name: 'Simulate', component: <Simulate /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Main: File list */}
      <Dock x={hx} y={0} w={1 - hx} h={vy}>
        <DockPanel dockId="r-main" panels={[
          { name: 'Data Files', component: <PreviewDataFileList /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Bottom: Chart + Table */}
      <Dock x={hx} y={vy} w={1 - hx} h={1 - vy}>
        <DockPanel dockId="r-bottom" panels={[
          { name: 'Chart', component: <PreviewChart /> },
          { name: 'Table', component: <TablePreview /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* HConnector at x=hx, left=[left], right=[main, bottom] */}
      <HConnector xRatio={hx} onDrag={setHx} />

      {/* VConnector at y=vy, spans right side only */}
      <VConnector yRatio={vy} onDrag={setVy} left={hx} width={1 - hx} />
    </div>
  );
}
