# Lets_Go_Server — Stateless C++ Application Server

The application server for the Lets Go platform. Each client connection is a bidirectional gRPC stream managed by a **ChatStreamContainerObject**; a MongoDB **change stream** injects new messages to the right user, and targeted reads fetch message bodies on demand. Designed for horizontal scale with one `GrpcServerImpl` per instance. :contentReference[oaicite:30]{index=30} :contentReference[oaicite:31]{index=31}

## Highlights
- **Async model:** completion-queue tags → thread pool; per-stream call data executes off the server thread. :contentReference[oaicite:32]{index=32}  
- **Stream lifecycle:** clear end reasons (timeout, shutdown, superseded by another device). :contentReference[oaicite:33]{index=33}  
- **Matching engine:** two-sided filters + point scoring, expiration windows, cooldowns/caps. :contentReference[oaicite:34]{index=34} :contentReference[oaicite:35]{index=35} :contentReference[oaicite:36]{index=36}  

<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="700">
</p>

## Chat Streaming (how it works)
- One **ChatStreamContainerObject** per connected user (lock-free with atomics/spinlocks). :contentReference[oaicite:37]{index=37}  
- Mongo **change stream** → injects new DB messages → user’s stream; on-demand lookups by msg id(s). :contentReference[oaicite:38]{index=38}  
- Ordering & reliability: tolerate duplicates to avoid misses; single-thread injection + short delay for better ordering. :contentReference[oaicite:39]{index=39}  

## Matching (overview)
- Filter both sides (age, genders, distance, activity/category overlap), then score matches; expiration depends on overlap/“anytime”/etc. :contentReference[oaicite:40]{index=40} :contentReference[oaicite:41]{index=41}  
- The server always ends the stream with a **cap** message (success/no-matches/cooldown/no-swipes). :contentReference[oaicite:42]{index=42}  

## Accounts & Auth (server view)
- **Login** returns tokens, timestamps, and server category/activity indexes; pending accounts use TTL. :contentReference[oaicite:43]{index=43} :contentReference[oaicite:44]{index=44}  
- **SMS Verification** supports: add installation to existing account (birthday check) **or** create fresh account; rationale: reused phone numbers. :contentReference[oaicite:45]{index=45} :contentReference[oaicite:46]{index=46}  

## Error Handling (admin-facing)
- Fresh errors are triaged; you can delete an instance or **mark a type “handled”** (moves all of that type and stops storing new ones). Indexed for fast queries. :contentReference[oaicite:47]{index=47} :contentReference[oaicite:48]{index=48}

## Data Model (selected)
- **Users**: phone/account ids + algo index (for matching). :contentReference[oaicite:49]{index=49}  
- **Pending Accounts**: unique fields + TTL index to auto-clean. :contentReference[oaicite:50]{index=50}

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
