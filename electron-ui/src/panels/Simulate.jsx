import React from 'react';
import { PanelContainer, Row, Label, Dropdown } from '../components/widgets';

export default function Simulate() {
  return (
    <PanelContainer>
      <Row>
        <Label text="Strategy" />
        <Dropdown
          options={['Random Walk', 'Strategy 2', 'Strategy 3']}
          stateKey="simulate_strategy"
          defaultValue="Random Walk"
        />
      </Row>
    </PanelContainer>
  );
}
