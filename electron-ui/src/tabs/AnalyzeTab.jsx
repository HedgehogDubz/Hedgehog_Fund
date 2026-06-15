import React, { useState } from 'react';
import { HConnector, VConnector, Dock } from '../components/TabLayout';
import DockPanel from '../components/DockPanel';
import AnalyzeDataFileList from '../panels/AnalyzeDataFileList';
import AlgorithmTradeList from '../panels/AlgorithmTradeList';
import AnalyzeChart from '../panels/AnalyzeChart';

// Tab_Analyze layout from TabDock:
//   left1: Dock(0,   0,   0.3, 0.5)   - Data files
//   left2: Dock(0,   0.5, 0.3, 0.5)   - Algorithms
//   right: Dock(0.3, 0,   0.7, 1)     - Chart
//   VConnector(0.5, [left1], [left2])
//   HConnector(0.3, [left1, left2], [right])
//
// +-----------+--------------------+
// | DataFiles |                    |
// |           |                    |
// +-----------+      Chart         |
// | Algos     |                    |
// |           |                    |
// +-----------+--------------------+

export default function AnalyzeTab() {
  const [hx, setHx] = useState(0.3);
  const [vy, setVy] = useState(0.5);

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      {/* Top-left: Data file list */}
      <Dock x={0} y={0} w={hx} h={vy}>
        <DockPanel dockId="a-top-left" panels={[
          { name: 'Data Files', component: <AnalyzeDataFileList /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Bottom-left: Algorithm trade list */}
      <Dock x={0} y={vy} w={hx} h={1 - vy}>
        <DockPanel dockId="a-bottom-left" panels={[
          { name: 'Algorithms', component: <AlgorithmTradeList /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Right: Chart */}
      <Dock x={hx} y={0} w={1 - hx} h={1}>
        <DockPanel dockId="a-right" panels={[
          { name: 'Chart', component: <AnalyzeChart /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* VConnector at y=vy, spans left side only */}
      <VConnector yRatio={vy} onDrag={setVy} left={0} width={hx} />

      {/* HConnector at x=hx, left=[left1, left2], right=[right] */}
      <HConnector xRatio={hx} onDrag={setHx} />
    </div>
  );
}
