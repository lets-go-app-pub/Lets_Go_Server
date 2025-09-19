# Lets_Go_Server — Stateless C++ Application Server

Application server for the Lets Go platform. It serves Android and Desktop Admin clients over **gRPC/Protobuf**, persists to a **MongoDB replica set**, streams chat in real time, and embeds a matching engine (Mongo aggregation → compiled C++ pipeline). I led the architecture and server operations.

> **Stack:** C++ · gRPC/Protobuf · MongoDB (Replica Set) · Linux (systemd) · SSL/TLS  
> **Clients & Integrations:** Android Client · Desktop Admin (Qt) · WordPress site · **Twilio** (SMS) · **SendGrid** (Email)

## Architecture

<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="820">
</p>

---

## Highlights (skim me)
- **Async, scalable core:** gRPC **completion queue** + thread pool; one lifecycle `GrpcServerImpl`; stateless handlers.
- **Reliable streaming chat:** per-user stream container; **Mongo change stream** fan-out; duplicate-tolerant with a small ordering window.
- **Fairness built-in:** writes are pushed to the **back** of the completion queue (alarms) so reads/heartbeats aren’t starved.
- **Declarative matching:** two-sided filters + point scoring (time windows, activity/category overlap, recency/inactivity penalties) with clear terminal **cap** messages.
- **Operations:** app nodes and Mongo RS on **separate Linux servers**; TLS, backups, and index bootstrap included.

---

## How it works

### Streaming (bidirectional chat)
- One **ChatStreamContainerObject** per connected user (lock-free with atomics/spinlocks).  
- A MongoDB **change stream** injects new messages/events into the user’s stream; targeted reads fill gaps by message id(s).  
- Prefer **duplicates** over misses; a brief buffer window plus single-thread injection improves apparent ordering.

### Matching (overview)
- **Filter both sides** (age, genders, distance) and require **category/activity overlap**.  
- **Score** candidates (window overlaps, short-window bonuses, “between” windows), subtracting for **inactivity** and **recent matches**.  
- Results **always end with a cap** indicating success, no matches, cooldown, or out of swipes.

### Runtime model
- **Async CQ + thread pool:** the server thread parks on the completion queue; per-call “call data” runs off-thread.  
- **Fairness:** writes posted behind reads/alarms (via CQ alarm).  
- **Stream lifecycle:** explicit down reasons (timeout, server shutdown, superseded by a newer device).

---

## What the server can do (Proto surface)
<details>
<summary>Expand to view .proto groups</summary>

**User/Auth & Session**  
`LoginFunction.proto`, `LoginSupportFunctions.proto`, `LoginToServerBasicInfo.proto`, `LoginValuesToReturnToClient.proto`, `PreLoginTimestamps.proto`, `AccountState.proto`, `UserAccountType.proto`, `AccountLoginTypeEnum.proto`, `AccessStatusEnum.proto`, `UserSubscriptionStatus.proto`

**SMS/Email & Verification**  
`SMSVerification.proto`, `EmailSendingMessages.proto`

**Chat & Rooms**  
`ChatMessageStream.proto`, `TypeOfChatMessage.proto`, `ChatMessageToClientMessage.proto`, `ChatRoomInfoMessage.proto`, `ChatRoomCommands.proto`, `CreatedChatRoomInfo.proto`, `UpdateOtherUserMessages.proto`, `MemberSharedInfoMessage.proto`

**Matching & Discovery**  
`FindMatches.proto`, `UserMatchOptions.proto`, `AlgorithmSearchOptions.proto`, `CategoryTimeFrame.proto`, `LetsGoEventStatus.proto`

**Requests & Data Access**  
`RequestMessages.proto`, `RequestFields.proto`, `RequestInformation.proto` (if present), `RetrieveServerLoad.proto`

**Feedback/Reporting/Errors**  
`FeedbackTypeEnum.proto`, `ReportMessages.proto`, `ErrorMessage.proto`, `ErrorOriginEnum.proto`, `SendErrorToServer.proto`

**Admin (Desktop Interface / Ops)**  
`AdminChatRoomCommands.proto`, `AdminEventCommands.proto`, `ManageServerCommands.proto`, `RequestAdminInfo.proto`, `RequestStatistics.proto`, `RequestUserAccountInfo.proto`, `SetAdminFields.proto`, `HandleErrors.proto`, `HandleReports.proto`, `HandleFeedback.proto`, `ErrorHandledMoveReasonEnum.proto`, `AdminLevelEnum.proto`, `UserAccountStatusEnum.proto`, `MatchTypeEnum.proto`, `AccountCategoryEnum.proto`

> Contracts live in: **Lets_Go_Profobuf** (shared `.proto` files).
</details>

---
## Data model (selected)
| Collection        | Purpose                         | Notable indexes / notes                            |
|-------------------|---------------------------------|----------------------------------------------------|
| `USER_ACCOUNTS`   | Account profile & algo fields   | `PHONE_NUMBER` (unique), `ACCOUNT_ID_LIST` (unique), algorithm index fields |
| `PENDING_ACCOUNT` | SMS verification staging        | uniques (`INDEXING`, `ID`, `PHONE_NUMBER`); **TTL** on verification timestamp |
| `MESSAGES` / `ROOMS` | Chat storage + membership   | room id + timestamps; observed by change streams   |
| `MATCHES`         | Candidate matches               | `userId`, `activityId`; recent-match recency idx   |
| `ERRORS`          | Fresh/handled errors            | compound indexes; mark-type-as-handled workflow    |

---

## Design decisions (why)
- **Stateless server nodes** for scale and easy rolling deploys; consistency in DB + tokens.  
- **Duplicate-tolerant streaming** favors liveness; client-side dedupe is cheap.  
- **CQ fairness** (write-behind) prevents chat stalls during heavy writes.  
- **Aggregation-first matching** keeps rules declarative; compiled C++ avoids BSON round-trips.  
- **Operational separation** (app vs data) isolates failures; Mongo **replica set** balances availability with simple ops for a small team.

---

## Code tour (where to look)

**`/server/src/globals/`** — connection pool, thread-pool handle, server flags; canonical DB field names; live stream counters; env & I/O helpers.  
**`/server/src/grpc_functions/`** — RPC handlers: chat stream & room commands; matching; login & SMS verification; request/read APIs; admin ops; email; load.  
**`/server/src/utility/`** — async CQ loop; change-stream & fan-out; matching document builder; room membership; account lifecycle; stats utilities; startup/index bootstrap; lock-free primitives; error capture; shared helpers.  
**`/server/testing/`** — account/media generators; mock gRPC stream; test scaffolding.  
**`/test/`** — mirrors `grpc_functions/` & `utility/` with fixtures, seeded docs, and helpers.  
**`/server/python/`** — small helper tools.  
**`/server/resources/`** — static assets/config.  
**`/obsolete/`** — legacy reference.

---

More explicit documentation can be found inside the project files titled **_documentation.md**.

## Other Related Repositories

- **Android Client (Kotlin)** — auth, profiles, activities, chat *(SDK versions may be dated)*  
  👉 [`Lets_Go_Android_Client`](https://github.com/lets-go-app-pub/Lets_Go_Android_Client)

- **Desktop Admin (Qt)** — admin/ops console for moderation, events, stats, and controls  
  👉 [`Lets_Go_Interface`](https://github.com/lets-go-app-pub/Lets_Go_Interface)

- **Matching (Algo & Converter)** — Mongo aggregation (JS) + C++ converter to embed pipelines  
  👉 [`Lets_Go_Algorithm_And_Conversion`](https://github.com/lets-go-app-pub/Lets_Go_Algorithm_And_Conversion)

- **Protobuf Files** — protobuf files used to communicate between server and clients  
  👉 [`Lets_Go_Profobuf`](https://github.com/lets-go-app-pub/Lets_Go_Profobuf)

## Status & compatibility
Portfolio reference for a completed system. Deployed on **separate Linux hosts** (app servers + MongoDB **replica set**) with TLS and backups. Toolchains may be dated.

## License
MIT
