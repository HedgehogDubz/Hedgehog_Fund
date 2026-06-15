import React, { useState, useEffect } from 'react';
import { PanelContainer, Row, Button, ListWidget } from '../components/widgets';
import { usePanelValue, useSetPanelValue } from '../state/store';
import { theme } from '../utils/theme';

export default function DataFileList({ listKey }) {
  const [files, setFiles] = useState([]);
  const selected = usePanelValue(listKey, []);
  const setValue = useSetPanelValue();

  const refreshFiles = async () => {
    try {
      const list = await window.api.listParquetFiles();
      setFiles(list);
    } catch (e) {
      console.error('Failed to list parquet files:', e);
    }
  };

  useEffect(() => {
    refreshFiles();
    const interval = setInterval(refreshFiles, 2000);
    return () => clearInterval(interval);
  }, []);

  const handleSelect = (sel) => {
    setValue(listKey, sel);
  };

  const handleDelete = async () => {
    if (!selected || selected.length === 0) return;
    const name = selected[0];
    try {
      await window.api.deleteParquetFile(name);
      setValue(listKey, []);
      await refreshFiles();
    } catch (e) {
      console.error('Failed to delete file:', e);
    }
  };

  const handleRename = async () => {
    if (!selected || selected.length === 0) return;
    const oldName = selected[0];
    const newName = window.prompt('Enter new name:', oldName);
    if (!newName || newName === oldName) return;
    try {
      await window.api.renameParquetFile(oldName, newName);
      setValue(listKey, [newName]);
      await refreshFiles();
    } catch (e) {
      console.error('Failed to rename file:', e);
    }
  };

  return (
    <PanelContainer>
      <Row>
        <Button text="Rename" onClick={handleRename} />
        <Button text="Delete" onClick={handleDelete} />
      </Row>
      <ListWidget
        items={files}
        selected={selected}
        onSelect={handleSelect}
      />
    </PanelContainer>
  );
}
