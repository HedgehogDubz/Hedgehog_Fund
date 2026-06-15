import React, { useState, useEffect } from 'react';
import { Row, Label, Dropdown, TextInput, Button, Separator, PanelContainer } from '../components/widgets';
import { usePanelValue, useSetPanelValue } from '../state/store';
import { INTERVALS, OFFER_SIDES, CATEGORIES, LOOKBACK_OPTIONS } from '../utils/constants';
import { theme } from '../utils/theme';

const LOOKBACK_DAYS = {
  '1 Day': 1,
  '3 Days': 3,
  '1 Week': 7,
  '2 Weeks': 14,
  '1 Month': 30,
  '3 Months': 90,
  '6 Months': 180,
  '1 Year': 365,
  '2 Years': 730,
  '5 Years': 1825,
};

function formatDate(date) {
  return date.toISOString().split('T')[0];
}

export default function Retrieve() {
  const setValue = useSetPanelValue();

  const mode = usePanelValue('retrieve_mode', 'Ticker');
  const ticker = usePanelValue('retrieve_ticker', 'AAPL');
  const category = usePanelValue('retrieve_category', 'FX Majors');
  const instrument = usePanelValue('retrieve_instrument', '');
  const dateMode = usePanelValue('retrieve_date_mode', 'Lookback');
  const lookback = usePanelValue('retrieve_lookback', '1 Month');
  const startText = usePanelValue('retrieve_start', '2025-01-01');
  const endText = usePanelValue('retrieve_end', '2025-02-01');
  const calStart = usePanelValue('retrieve_cal_start', '');
  const calEnd = usePanelValue('retrieve_cal_end', '');
  const intervalLabel = usePanelValue('retrieve_interval', '1 Hour');
  const offerSideLabel = usePanelValue('retrieve_offer_side', 'Bid');

  const [instruments, setInstruments] = useState([]);
  const [status, setStatus] = useState('');
  const [loading, setLoading] = useState(false);

  // Fetch instruments when category changes
  useEffect(() => {
    let cancelled = false;
    if (mode === 'Browse' && window.api?.getInstruments) {
      window.api.getInstruments(category).then((result) => {
        if (!cancelled) {
          const list = result || [];
          setInstruments(list);
          if (list.length > 0 && !list.includes(instrument)) {
            setValue('retrieve_instrument', list[0]);
          }
        }
      }).catch(() => {
        if (!cancelled) setInstruments([]);
      });
    }
    return () => { cancelled = true; };
  }, [category, mode]);

  const computeDates = () => {
    const now = new Date();
    let start, end;

    if (dateMode === 'Lookback') {
      const days = LOOKBACK_DAYS[lookback] || 30;
      const startDate = new Date(now);
      startDate.setDate(startDate.getDate() - days);
      start = formatDate(startDate);
      end = formatDate(now);
    } else if (dateMode === 'Text') {
      start = startText;
      end = endText;
    } else if (dateMode === 'Calendar') {
      start = calStart;
      end = calEnd;
    }

    return { start, end };
  };

  const handleFetch = async () => {
    setLoading(true);
    setStatus('Fetching...');

    try {
      const { start, end } = computeDates();

      if (!start || !end) {
        setStatus('Error: start and end dates are required.');
        setLoading(false);
        return;
      }

      let resolvedInstrument;
      if (mode === 'Ticker') {
        resolvedInstrument = await window.api.resolveTicker(ticker);
        if (!resolvedInstrument) {
          setStatus(`Error: could not resolve ticker "${ticker}".`);
          setLoading(false);
          return;
        }
      } else {
        resolvedInstrument = instrument;
        if (!resolvedInstrument) {
          setStatus('Error: no instrument selected.');
          setLoading(false);
          return;
        }
      }

      if (!window.api?.fetchData) {
        setStatus('Error: fetchData API not available.');
        setLoading(false);
        return;
      }

      await window.api.fetchData({
        instrument: resolvedInstrument,
        intervalLabel,
        offerSideLabel,
        start,
        end,
      });

      setStatus('Fetch complete.');
    } catch (err) {
      setStatus(`Error: ${err.message || err}`);
    } finally {
      setLoading(false);
    }
  };

  const dateInputStyle = {
    background: theme.inputBg,
    color: theme.textColor,
    border: `1px solid ${theme.borderDark}`,
    borderRadius: 3,
    padding: '5px 8px',
    fontSize: 12,
    fontFamily: theme.fontUI,
    outline: 'none',
    width: '100%',
    boxSizing: 'border-box',
    boxShadow: 'inset 0 1px 0 rgba(0,0,0,0.25)',
  };

  return (
    <PanelContainer>
      {/* Mode Selection */}
      <Row>
        <Label text="Mode" />
        <Dropdown
          options={['Ticker', 'Browse']}
          stateKey="retrieve_mode"
          defaultValue="Ticker"
          style={{ flex: 1 }}
        />
      </Row>

      {/* Ticker Mode */}
      {mode === 'Ticker' && (
        <Row>
          <Label text="Ticker" />
          <TextInput
            stateKey="retrieve_ticker"
            defaultValue="AAPL"
            placeholder="AAPL"
            style={{ flex: 1 }}
          />
        </Row>
      )}

      {/* Browse Mode */}
      {mode === 'Browse' && (
        <>
          <Row>
            <Label text="Category" />
            <Dropdown
              options={Object.keys(CATEGORIES)}
              stateKey="retrieve_category"
              defaultValue="FX Majors"
              style={{ flex: 1 }}
            />
          </Row>
          <Row>
            <Label text="Instrument" />
            <Dropdown
              options={instruments}
              stateKey="retrieve_instrument"
              style={{ flex: 1 }}
            />
          </Row>
        </>
      )}

      {/* Date Mode */}
      <Separator />
      <Row>
        <Label text="Date Mode" />
        <Dropdown
          options={['Lookback', 'Text', 'Calendar']}
          stateKey="retrieve_date_mode"
          defaultValue="Lookback"
          style={{ flex: 1 }}
        />
      </Row>

      {/* Lookback */}
      {dateMode === 'Lookback' && (
        <Row>
          <Label text="Look into the past" />
          <Dropdown
            options={LOOKBACK_OPTIONS}
            stateKey="retrieve_lookback"
            defaultValue="1 Month"
            style={{ flex: 1 }}
          />
        </Row>
      )}

      {/* Text dates */}
      {dateMode === 'Text' && (
        <>
          <Row>
            <Label text="Start" />
            <TextInput
              stateKey="retrieve_start"
              defaultValue="2025-01-01"
              placeholder="2025-01-01"
              style={{ flex: 1 }}
            />
          </Row>
          <Row>
            <Label text="End" />
            <TextInput
              stateKey="retrieve_end"
              defaultValue="2025-02-01"
              placeholder="2025-02-01"
              style={{ flex: 1 }}
            />
          </Row>
        </>
      )}

      {/* Calendar dates */}
      {dateMode === 'Calendar' && (
        <>
          <Row>
            <Label text="Start" />
            <input
              type="date"
              value={calStart || ''}
              onChange={(e) => setValue('retrieve_cal_start', e.target.value)}
              style={{ ...dateInputStyle, flex: 1 }}
            />
          </Row>
          <Row>
            <Label text="End" />
            <input
              type="date"
              value={calEnd || ''}
              onChange={(e) => setValue('retrieve_cal_end', e.target.value)}
              style={{ ...dateInputStyle, flex: 1 }}
            />
          </Row>
        </>
      )}

      {/* Options */}
      <Separator />
      <Row>
        <Label text="Interval" />
        <Dropdown
          options={Object.keys(INTERVALS)}
          stateKey="retrieve_interval"
          defaultValue="1 Hour"
          style={{ flex: 1 }}
        />
      </Row>
      <Row>
        <Label text="Offer Side" />
        <Dropdown
          options={Object.keys(OFFER_SIDES)}
          stateKey="retrieve_offer_side"
          defaultValue="Bid"
          style={{ flex: 1 }}
        />
      </Row>

      {/* Fetch */}
      {status && (
        <Row>
          <Label
            text={status}
            style={{ color: status.startsWith('Error') ? theme.errorColor : theme.successColor }}
          />
        </Row>
      )}
      <Row>
        <Button
          text={loading ? 'Fetching...' : 'Fetch'}
          onClick={handleFetch}
          primary
          disabled={loading}
          style={{ flex: 1 }}
        />
      </Row>
    </PanelContainer>
  );
}
