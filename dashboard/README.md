# Dashboard (Full-Stack Component)

Real-time web dashboard for the exchange simulator.

## Owner

This component is owned by the full-stack team member.

## Features to Implement

### Phase 1: Basic Visualization
- [ ] Real-time order book display (bid/ask ladder)
- [ ] Trade tape (recent trades)
- [ ] Basic connection to backend WebSocket

### Phase 2: Controls
- [ ] Start/stop simulation
- [ ] Add/remove symbols
- [ ] Agent configuration panel
- [ ] Scenario selection

### Phase 3: Analytics
- [ ] Latency histogram charts
- [ ] Throughput graphs
- [ ] PnL tracking per agent
- [ ] Volume charts

### Phase 4: Advanced
- [ ] Parameter sweep interface
- [ ] Report export (CSV/JSON)
- [ ] Replay controls
- [ ] User authentication

## Tech Stack Recommendations

### Frontend
- **React** with TypeScript
- **TailwindCSS** for styling
- **Recharts** or **Plotly** for charts
- **WebSocket** for real-time data

### Backend for Dashboard
- **Node.js** with Express (or Fastify)
- **WebSocket** (ws package)
- **Protocol Buffers** or **FlatBuffers** for efficient serialization

## API Contract

### WebSocket Streams

```
ws://localhost:8080/market-data
```

Messages (JSON or binary):
```json
// BBO Update
{
  "type": "bbo",
  "symbol": "AAPL",
  "bid": 150.25,
  "bid_qty": 100,
  "ask": 150.30,
  "ask_qty": 150,
  "ts": 1703123456789
}

// Trade
{
  "type": "trade",
  "symbol": "AAPL",
  "price": 150.27,
  "qty": 50,
  "side": "buy",
  "ts": 1703123456790
}

// Depth Update (L2)
{
  "type": "depth",
  "symbol": "AAPL",
  "bids": [[150.25, 100], [150.20, 200], ...],
  "asks": [[150.30, 150], [150.35, 100], ...],
  "ts": 1703123456791
}
```

### REST Control Plane

```
POST /api/simulation/start
POST /api/simulation/stop
GET  /api/simulation/status

POST /api/symbols
  {"symbol": "AAPL"}
DELETE /api/symbols/:symbol

GET  /api/agents
POST /api/agents
  {"type": "market_maker", "symbol": "AAPL", "config": {...}}
DELETE /api/agents/:id

GET  /api/metrics
GET  /api/metrics/latency
GET  /api/metrics/throughput
```

## Getting Started

```bash
# Install dependencies
npm install

# Development
npm run dev

# Build
npm run build

# Start production
npm start
```

## Directory Structure

```
dashboard/
├── src/
│   ├── components/
│   │   ├── OrderBook.tsx
│   │   ├── TradeTape.tsx
│   │   ├── LatencyChart.tsx
│   │   └── AgentControls.tsx
│   ├── hooks/
│   │   ├── useWebSocket.ts
│   │   └── useMarketData.ts
│   ├── lib/
│   │   ├── api.ts
│   │   └── types.ts
│   ├── App.tsx
│   └── main.tsx
├── public/
├── package.json
└── README.md
```

## Design Notes

### Order Book Visualization
- Green for bids, red for asks
- Size bars proportional to quantity
- Spread clearly visible
- Update animations for changes

### Performance Considerations
- Throttle updates to 60fps max
- Use virtual scrolling for trade tape
- Memoize components
- Consider WebGL for high-frequency updates

### Color Scheme
- Dark theme preferred for trading UIs
- Clear contrast for bid/ask
- Subtle animations
