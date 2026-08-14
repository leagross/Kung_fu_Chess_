# Deploying kfc_server to a real cloud host

This is [Roadmap.md](../Roadmap.md)'s stage 1: the same `kfc_server` image,
running on a machine that is actually on the internet, behind TLS. Nothing
here needs Kubernetes, Redis or a second language -- just this repo's
existing `Dockerfile`, plus [Caddy](https://caddyserver.com) in front of it
for `wss://` and an automatic Let's Encrypt certificate.

Two files:
- **`Caddyfile`** -- routes `/api/*` to kfc_server's HTTP API (port 8081) and
  everything else to its WebSocket game protocol (port 8080), and gets its
  own TLS certificate for the domain you point here.
- **`compose.prod.yaml`** -- a standalone compose file (not merged with the
  root `compose.yaml`, which is for local development) that runs `server`
  with no host ports published at all, and `caddy` as the only thing facing
  the internet, on 80/443.

## Steps

1. **Rent a Linux VM** from any cloud provider (DigitalOcean, AWS EC2 t3.micro,
   Oracle's free tier, Hetzner, ...) and install Docker + the compose plugin
   on it.
2. **Point a domain (or subdomain) at the VM's IP** -- an `A` record. Caddy
   needs this to exist *before* it requests a certificate.
3. **Copy this repo onto the VM** (`git clone`, or `scp`/`rsync` a tarball).
4. **Set the domain**: create `DOCKER/.env` on the VM with
   ```
   DOMAIN=chess.example.com
   ```
5. **Open only 80 and 443** in the VM's firewall/security group (not 8080,
   not 8081 -- those are never published; see `compose.prod.yaml`).
6. **Run it**:
   ```sh
   cd DOCKER
   docker compose -f compose.prod.yaml up --build -d
   ```
   First start takes a minute longer than usual: Caddy is requesting and
   installing the certificate.
7. **Point clients at it**: `wss://chess.example.com` instead of
   `ws://localhost:8080`, and `https://chess.example.com/api/...` instead of
   `http://localhost:8081/api/...`.

## Acceptance test

Two different physical machines, on two different networks, running
`kfc_gui_app --server=wss://chess.example.com`, playing a real game against
each other. That is the actual bar for "the server is in the cloud" -- not
just that the containers start.

## Hardening

The other half of `Roadmap.md`'s stage 1 -- server-side changes, not
deployment configuration, so they live in `kfc_server` itself rather than
this folder:

- **Message size cap** -- already existed (`kMaxMessageBytes`, `ClientSession::on_text`).
- **Message-rate cap** -- `kMaxMessagesPerSecond` in `client_session.hpp`, checked the same way.
- **Idle-connection timeout** -- IXWebSocket's own ping/pong, enabled in `WebSocketGameServer`'s connection callback (`kIdlePingIntervalSecs`).
- **Graceful shutdown** -- `main.cpp` now traps SIGINT/SIGTERM and runs the existing orderly `rooms.stop_all()` teardown instead of a hard kill; matters for `docker stop`, which sends SIGTERM.
- **Health check** -- `GET /health` on the HTTP API port.
- **Log rotation** -- `FileLogger` rotates `kfc_server.log` to `.1` past a size cap instead of growing forever.
- **No password on the command line** -- `kfc_gui_app` prompts for it instead of taking `--password=`.

All done and covered by `kfc_tests`. Config-and-env hardening (secrets via
env vars rather than baked in, which this repo already does for
`gameplay.json`'s path and the server's port) was already true before this
folder existed.
