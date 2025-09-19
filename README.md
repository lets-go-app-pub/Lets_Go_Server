# Lets_Go_Server — Stateless C++ Application Server

Application server for the Lets Go platform. It serves Android and Desktop Admin clients over **gRPC/Protobuf**, persists to a **MongoDB replica set**, streams chat in real time, and embeds a matching engine (Mongo aggregation → compiled C++ pipeline). I led the architecture and implementation of the server and operations.

> **Stack:** C++ · gRPC/Protobuf · MongoDB (Replica Set) · Linux (systemd) · SSL/TLS  
> **Clients & Integrations:** Android Client · Desktop Admin (Qt) · WordPress site · Twilio (SMS) · SendGrid (Email)

## Architecture

<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="820">
</p>

---

## Highlights (skim me)
- **Async, scalable core:** gRPC **completion queue** + thread pool; one lifecycle `GrpcServerImpl` per process; stateless request handlers.
- **Reliable streaming chat:** one stream container per user; **Mongo change stream** injects live messages; tolerant of duplicates to avoid misses; lightweight ordering window.
- **Fairness built-in:** writes are intentionally pushed to the **back** of the completion queue (via alarms) so reads/heartbeats aren’t starved.
- **Declarative matching:** two-sided filters + point scoring (time windows, category/activity overlap, recency/inactivity penalties) with clear terminal “cap” messages (success/no-matches/cooldown/no-swipes).
- **Operational clarity:** app nodes and Mongo RS run on **separate Linux servers**; TLS termination, backups, and index bootstrap handled in repo.

---

## What the server can do (Proto surface at a glance)

**User/Auth & Session**
- `LoginFunction.proto`, `LoginSupportFunctions.proto`, `LoginToServerBasicInfo.proto`, `LoginValuesToReturnToClient.proto`, `PreLoginTimestamps.proto`, `AccountState.proto`, `UserAccountType.proto`, `AccountLoginTypeEnum.proto`, `AccessStatusEnum.proto`, `UserSubscriptionStatus.proto`

**SMS/Email & Verification**
- `SMSVerification.proto`, `EmailSendingMessages.proto`

**Chat & Rooms**
- `ChatMessageStream.proto`, `TypeOfChatMessage.proto`, `ChatMessageToClientMessage.proto`, `ChatRoomInfoMessage.proto`, `ChatRoomCommands.proto`, `CreatedChatRoomInfo.proto`, `UpdateOtherUserMessages.proto`, `MemberSharedInfoMessage.proto`

**Matching & Discovery**
- `FindMatches.proto`, `UserMatchOptions.proto`, `AlgorithmSearchOptions.proto`, `CategoryTimeFrame.proto`, `LetsGoEventStatus.proto`

**Requests & Data Access**
- `RequestMessages.proto`, `RequestFields.proto`, `RequestInformation.proto` (if present), `RetrieveServerLoad.proto`

**Feedback/Reporting/Errors**
- `FeedbackTypeEnum.proto`, `ReportMessages.proto`, `ErrorMessage.proto`, `ErrorOriginEnum.proto`, `SendErrorToServer.proto`

**Admin (Desktop Interface / Ops)**
- `AdminChatRoomCommands.proto`, `AdminEventCommands.proto`, `ManageServerCommands.proto`, `RequestAdminInfo.proto`, `RequestStatistics.proto`, `RequestUserAccountInfo.proto`, `SetAdminFields.proto`, `HandleErrors.proto`, `HandleReports.proto`, `HandleFeedback.proto`, `ErrorHandledMoveReasonEnum.proto`, `AdminLevelEnum.proto`, `UserAccountStatusEnum.proto`, `MatchTypeEnum.proto`, `AccountCategoryEnum.proto`

> Contracts live in their own repo: **Lets_Go_Profobuf** (shared `.proto` files).  

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

---

## How it works

### Chat streaming (bidirectional)
- **Per-user stream container:** Each connected user gets a container object managing the bidirectional stream; it uses a lock-free design (atomics/spinlocks where needed).
- **Live message fan-out:** A MongoDB **change stream** thread watches relevant collections and injects new messages/events into the user’s stream; targeted reads fill gaps by message id(s).
- **Ordering & reliability:** The server tolerates **duplicates** rather than miss events, and uses a small **buffer window** plus single-thread injection to improve apparent ordering.

### Matching (overview)
- **Filter both sides** first (age, genders, distance) and require **category/activity overlap**; then **score** candidates (window overlaps, short-window bonuses, “between” windows), subtracting for **inactivity** and **recent matches**.
- Results **always end with a cap** message that tells the client the terminal state (success, no matches, cooldown required, out of swipes).

### Server runtime model
- **Async CQ + thread pool:** the gRPC server thread parks on the completion queue; per-call “call data” executes off-thread.
- **Fairness:** writes are posted behind reads/alarms (via CQ alarm) so long-running writes don’t starve the stream.
- **Stream lifecycle:** explicit down reasons (timeout, server shutdown, superseded by a newer device connection).

---

## Other Related Repositories

- **Android Client (Kotlin)** — auth, profiles, activities, chat *(SDK versions may be dated)*  
  👉 [`Lets_Go_Android_Client`](https://github.com/lets-go-app-pub/Lets_Go_Android_Client)

- **Desktop Admin (Qt)** — admin/ops console for moderation, events, stats, and controls  
  👉 [`Lets_Go_Interface`](https://github.com/lets-go-app-pub/Lets_Go_Interface)

- **Matching (Algo & Converter)** — Mongo aggregation (JS) + C++ converter to embed pipelines  
  👉 [`Lets_Go_Algorithm_And_Conversion`](https://github.com/lets-go-app-pub/Lets_Go_Algorithm_And_Conversion)

- **Protobuf Files** — protobuf files used to communicate between server and clients  
  👉 [`Lets_Go_Profobuf`](https://github.com/lets-go-app-pub/Lets_Go_Profobuf)


## License
MIT
