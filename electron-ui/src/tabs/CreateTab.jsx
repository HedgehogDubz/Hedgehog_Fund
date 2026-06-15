import React, { useState } from 'react';
import { HConnector, VConnector, Dock } from '../components/TabLayout';
import DockPanel from '../components/DockPanel';
import CreateFileList from '../panels/CreateFileList';
import IDE from '../panels/IDE';
import Console from '../panels/Console';

// Tab_Create layout from TabDock:
//   left:         Dock(0,   0,    0.2,  1)
//   top_right:    Dock(0.2, 0,    0.8,  0.75)
//   bottom_right: Dock(0.2, 0.75, 0.8,  0.25)
//   HConnector(0.2, [left], [top_right, bottom_right])
//   VConnector(0.75, [top_right], [bottom_right])
//
// +--------+--------------------+
// |        |       IDE          |
// | Files  |                    |
// |        +--------------------+
// |        |     Console        |
// +--------+--------------------+

export default function CreateTab() {
  const [hx, setHx] = useState(0.2);
  const [vy, setVy] = useState(0.75);

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      {/* Left: File tree */}
      <Dock x={0} y={0} w={hx} h={1}>
        <DockPanel dockId="c-left" panels={[
          { name: 'Files', component: <CreateFileList /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Top right: IDE */}
      <Dock x={hx} y={0} w={1 - hx} h={vy}>
        <DockPanel dockId="c-ide" panels={[
          { name: 'IDE', component: <IDE /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* Bottom right: Console */}
      <Dock x={hx} y={vy} w={1 - hx} h={1 - vy}>
        <DockPanel dockId="c-console" panels={[
          { name: 'Console', component: <Console /> },
        ]} style={{ height: '100%' }} />
      </Dock>

      {/* HConnector at x=hx, left=[left], right=[top_right, bottom_right] */}
      <HConnector xRatio={hx} onDrag={setHx} />

      {/* VConnector at y=vy, spans right side only */}
      <VConnector yRatio={vy} onDrag={setVy} left={hx} width={1 - hx} />
    </div>
  );
}
