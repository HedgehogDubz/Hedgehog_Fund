from datetime import datetime
from pathlib import Path
import dukascopy_python as dk
import dukascopy_python.instruments as dk_instruments

DATA_DIR = Path(__file__).parent / "data"

INTERVALS = {
    "Tick":    dk.INTERVAL_TICK,
    "1 Sec":   dk.INTERVAL_SEC_1,
    "10 Sec":  dk.INTERVAL_SEC_10,
    "30 Sec":  dk.INTERVAL_SEC_30,
    "1 Min":   dk.INTERVAL_MIN_1,
    "5 Min":   dk.INTERVAL_MIN_5,
    "10 Min":  dk.INTERVAL_MIN_10,
    "15 Min":  dk.INTERVAL_MIN_15,
    "30 Min":  dk.INTERVAL_MIN_30,
    "1 Hour":  dk.INTERVAL_HOUR_1,
    "4 Hour":  dk.INTERVAL_HOUR_4,
    "1 Day":   dk.INTERVAL_DAY_1,
    "1 Week":  dk.INTERVAL_WEEK_1,
    "1 Month": dk.INTERVAL_MONTH_1,
}

OFFER_SIDES = {
    "Bid": dk.OFFER_SIDE_BID,
    "Ask": dk.OFFER_SIDE_ASK,
}

CATEGORIES = {
    "FX Majors": "FX_MAJORS",
    "FX Crosses": "FX_CROSSES",
    "FX Minors": "FX_MINORS",
    "FX Exotics": "FX_EXOTICS",
    "FX Metals": "FX_METALS",
    "Indices": "IDX_",
    "Commodities": "CMD_",
    "ETFs": "ETF_",
    "Bonds": "BND_",
    "Stocks": None,
}


def _build_instrument_map():
    all_attrs = [x for x in dir(dk_instruments) if x.startswith("INSTRUMENT_")]
    categorized_keys = set()
    by_category = {}

    for cat_label, cat_filter in CATEGORIES.items():
        if cat_filter is None:
            continue
        items = {}
        for attr in all_attrs:
            if cat_filter in attr:
                val = getattr(dk_instruments, attr)
                items[val] = attr
                categorized_keys.add(attr)
        by_category[cat_label] = items

    stocks = {}
    for attr in all_attrs:
        if attr not in categorized_keys:
            val = getattr(dk_instruments, attr)
            stocks[val] = attr
    by_category["Stocks"] = stocks
    return by_category


def _build_ticker_map():
    all_attrs = [x for x in dir(dk_instruments) if x.startswith("INSTRUMENT_")]
    mapping = {}
    for attr in all_attrs:
        val = getattr(dk_instruments, attr)
        short = val.split(".")[0].split("/")[0].upper()
        mapping[short] = val
    return mapping


INSTRUMENT_MAP = _build_instrument_map()
TICKER_MAP = _build_ticker_map()


def get_instruments_for_category(category: str) -> list[str]:
    return sorted(INSTRUMENT_MAP.get(category, {}).keys())


def resolve_ticker(ticker: str) -> str | None:
    ticker = ticker.strip().upper()
    if ticker in TICKER_MAP:
        return TICKER_MAP[ticker]
    for val in TICKER_MAP.values():
        if val.upper() == ticker:
            return val
    return None


def fetch_data(instrument: str, interval_label: str, offer_side_label: str,
               start: datetime, end: datetime) -> str:
    interval = INTERVALS[interval_label]
    offer_side = OFFER_SIDES[offer_side_label]

    df = dk.fetch(instrument, interval, offer_side, start, end)

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    safe_name = instrument.replace("/", "_").replace("&", "_")
    filename = f"{safe_name}_{interval_label.replace(' ', '')}_{offer_side_label}_{start:%Y%m%d}_{end:%Y%m%d}.parquet"
    filepath = DATA_DIR / filename
    df.to_parquet(filepath, index=False)
    return str(filepath)


def list_parquet_files() -> list[str]:
    if not DATA_DIR.exists():
        return []
    return sorted(f.name for f in DATA_DIR.glob("*.parquet"))


def delete_parquet_file(filename: str) -> bool:
    filepath = DATA_DIR / filename
    if filepath.exists():
        filepath.unlink()
        return True
    return False


def rename_parquet_file(old_name: str, new_name: str) -> bool:
    if not new_name.endswith(".parquet"):
        new_name += ".parquet"
    old_path = DATA_DIR / old_name
    new_path = DATA_DIR / new_name
    if old_path.exists() and not new_path.exists():
        old_path.rename(new_path)
        return True
    return False


def read_parquet_preview(filename: str, max_rows: int = 100):
    import pandas as pd
    filepath = DATA_DIR / filename
    if not filepath.exists():
        return None
    df = pd.read_parquet(filepath)
    return df.head(max_rows)
