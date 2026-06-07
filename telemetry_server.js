#!/usr/bin/env node
'use strict';

const net = require('net');

function parseArgs(argv) {
  const args = {
    host: '127.0.0.1',
    port: 7000,
    eventHost: '127.0.0.1',
    eventPort: 6000,
    interval: 1000,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--host' && i + 1 < argv.length) {
      args.host = argv[++i];
    } else if (arg === '--port' && i + 1 < argv.length) {
      args.port = Number(argv[++i]);
    } else if (arg === '--event-host' && i + 1 < argv.length) {
      args.eventHost = argv[++i];
    } else if (arg === '--event-port' && i + 1 < argv.length) {
      args.eventPort = Number(argv[++i]);
    } else if (arg === '--interval' && i + 1 < argv.length) {
      args.interval = Number(argv[++i]) * 1000;
    } else {
      console.error('usage: telemetry_server.js [--host 127.0.0.1] [--port 7000] [--event-host 127.0.0.1] [--event-port 6000] [--interval 1]');
      process.exit(2);
    }
  }

  if (!Number.isInteger(args.port) || args.port < 1 || args.port > 65535) {
    console.error('invalid --port');
    process.exit(2);
  }
  if (!Number.isInteger(args.eventPort) || args.eventPort < 1 || args.eventPort > 65535) {
    console.error('invalid --event-port');
    process.exit(2);
  }
  if (!Number.isFinite(args.interval) || args.interval <= 0) {
    console.error('invalid --interval');
    process.exit(2);
  }
  return args;
}

function telemetryPayload() {
  const now = new Date();
  const seconds = now.getTime() / 1000;
  const date = now.toISOString().slice(0, 10);
  const time = now.toTimeString().slice(0, 8);
  const angle = Math.sin(seconds * 0.7) * 90;

  return {
    type: 'telemetry',
    angle: Number(angle.toFixed(2)),
    rotate: Number(((seconds * 8) % 360).toFixed(0)),
    azimuth: Number(((seconds * 10) % 360).toFixed(2)),
    heading: Math.trunc((seconds * 15) % 360),
    battery: Number((12.6 - ((seconds / 20) % 1.4)).toFixed(2)),
    date,
    time,
  };
}

function main() {
  const args = parseArgs(process.argv);
  const server = net.createServer((socket) => {
    const remote = `${socket.remoteAddress}:${socket.remotePort}`;
    console.log(`client connected: ${remote}`);

    const timer = setInterval(() => {
      const line = JSON.stringify(telemetryPayload());
      socket.write(`${line}\n`);
    }, args.interval);

    socket.on('close', () => {
      clearInterval(timer);
      console.log('client disconnected');
    });
    socket.on('error', (err) => {
      clearInterval(timer);
      console.error(`client error: ${err.message}`);
    });
  });

  server.on('error', (err) => {
    console.error(`server error: ${err.message}`);
    process.exit(1);
  });

  server.listen(args.port, args.host, () => {
    console.log(`telemetry server listening on ${args.host}:${args.port}`);
  });

  const eventServer = net.createServer((socket) => {
    const remote = `${socket.remoteAddress}:${socket.remotePort}`;
    let buffer = '';
    console.log(`event client connected: ${remote}`);

    socket.on('data', (chunk) => {
      buffer += chunk.toString('utf8');
      const lines = buffer.split('\n');
      buffer = lines.pop();

      for (const line of lines) {
        const text = line.trim();
        if (!text) {
          continue;
        }
        try {
          console.log('event:', JSON.parse(text));
        } catch (err) {
          console.log(`event raw: ${text}`);
        }
      }
    });

    socket.on('close', () => {
      console.log('event client disconnected');
    });
    socket.on('error', (err) => {
      console.error(`event client error: ${err.message}`);
    });
  });

  eventServer.on('error', (err) => {
    console.error(`event server error: ${err.message}`);
    process.exit(1);
  });

  eventServer.listen(args.eventPort, args.eventHost, () => {
    console.log(`event server listening on ${args.eventHost}:${args.eventPort}`);
  });
}

main();
