# Lets_Go_Server — Stateless C++ Application Server

The application server for the Lets Go platform. Each client connection is a bidirectional gRPC stream managed by a **ChatStreamContainerObject**; a MongoDB **change stream** injects new messages to the right user, and targeted reads fetch message bodies on demand. Designed for horizontal scale with one `GrpcServerImpl` per instance.

## Highlights
- **Async model:** completion-queue tags → thread pool; per-stream call data executes off the server thread.
- **Stream lifecycle:** clear end reasons (timeout, shutdown, superseded by another device).
- **Matching engine:** two-sided filters + point scoring, expiration windows, cooldowns/caps.

## Architecture

---

<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="820">
</p>

---

## Chat Streaming (how it works)
- One **ChatStreamContainerObject** per connected user (lock-free with atomics/spinlocks).
- Mongo **change stream** → injects new DB messages → user’s stream; on-demand lookups by msg id(s).
- Ordering & reliability: tolerate duplicates to avoid misses; single-thread injection + short delay for better ordering.

## Matching (overview)
- Filter both sides (age, genders, distance, activity/category overlap), then score matches; expiration depends on overlap/“anytime”/etc.
- The server always ends the stream with a **cap** message (success/no-matches/cooldown/no-swipes).

## Accounts & Auth (server view)
- **Login** returns tokens, timestamps, and server category/activity indexes; pending accounts use TTL.
- **SMS Verification** supports: add installation to existing account (birthday check) **or** create fresh account; rationale: reused phone numbers.

## Error Handling (admin-facing)
- Fresh errors are triaged; you can delete an instance or **mark a type “handled”** (moves all of that type and stops storing new ones). Indexed for fast queries.

## Data Model (selected)
- **Users**: phone/account ids + algo index (for matching).
- **Pending Accounts**: unique fields + TTL index to auto-clean.

## Ops (summary)
Separate Linux hosts for the **app server** and **MongoDB replica set**; systemd service, TLS, backups/restore, basic health metrics.

## Repo Tour
- `/src/server` – gRPC service impls  
- `/src/db` – Mongo client, repos, index bootstrap  
- `/src/matching` – aggregation→C++ converter  
- `/config` – TLS & sample configs  

> **Build/Run:** Legacy notes only; modern toolchains may require updates.

## License
MIT
