# RelationService

## Responsibilities

RelationService manages friend requests, friendships, and group membership through MySQL DAOs.

## Dependencies

```text
RelationServiceImpl
  +--> UserDao
  +--> RelationDao
  +--> FriendRequestDao
  +--> GroupDao
  +--> MySqlConnectionPool
```

`RelationServiceContext` owns config and DAO lifecycles. The service implementation only receives injected dependencies.

## Friend flows

The public friendship workflow is request based:

```text
SendFriendRequest -> pending
AcceptFriendRequest -> update request + insert bidirectional friendships in one transaction
RejectFriendRequest -> rejected
ListFriendRequests -> incoming/outgoing request views
```

Direct AddFriend is not a production friendship write path:

```text
AddFriend -> FRIEND_REQUEST_REQUIRED
```

Direct AddFriend no longer creates friendship rows in the production path. The RPC remains in the current proto surface but is disabled by policy; callers must use `SendFriendRequest` and `AcceptFriendRequest` to create friendships.

DeleteFriend checks relation existence and marks both rows inactive in one transaction.

ListFriends reads friend IDs and then queries UserDao for public UserInfo. This is currently N+1 and can later be optimized with `batchGetUsersByIds`.

## Group flows

CreateGroup validates owner and group name, then GroupDao transaction inserts `groups` and owner membership with role `1`.

JoinGroup validates user/group existence and rejects duplicate membership with `GROUP_ALREADY_JOINED`.

LeaveGroup rejects non-members and forbids owner leaving in this phase with `GROUP_OWNER_CANNOT_LEAVE`.

ListGroupMembers reads member IDs and returns public UserInfo.

## Schema notes

`friendships(user_id, friend_id)` unique index prevents duplicate edges. `group_members(group_id, user_id)` unique index prevents duplicate membership. Groups and members are split so group metadata and membership scale independently.

## Run

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
```

```bash
./build/relation_service/nebula_relation_service --config config/nebula.conf
./build/examples/relation_service_client --addr 127.0.0.1:50053
```

## Tests

```bash
./build/tests/test_relation_dao_extra
./build/tests/test_group_dao_extra
./build/tests/test_relation_service_impl
./build/tests/test_relation_service_integration
```

## Interview points

1. Friend relation can be stored as one bidirectional logical edge or two directional rows.
2. Two-row design makes listFriends simple but needs transaction consistency.
3. Direct AddFriend is rejected with `FRIEND_REQUEST_REQUIRED`; the public write path is request approval.
4. `user_id + friend_id` unique index still prevents duplicate friendship rows after request acceptance.
5. DeleteFriend updates both directional rows.
6. Groups and group_members are separate for normalization and scalable membership.
7. Friend request unique indexes prevent duplicate pending requests.
8. `group_id + user_id` unique index prevents duplicate joins.
9. Owner leave can transfer ownership, dissolve group, or be forbidden; this implementation forbids it.
10. N+1 means each friend/member causes one user query.
11. batchGetUsersByIds can query all user rows with `WHERE id IN (...)`.
12. RelationService currently reads user table directly; in stricter microservices it could call UserService.
13. Cross-service consistency can use local transactions, events, or sagas.
14. SendFriendRequest is intentionally separate from friendship creation so audit/moderation can be added later.
15. JoinGroup can be idempotent, but this implementation returns `GROUP_ALREADY_JOINED`.
16. A fuller production friend system could add block lists and moderation/audit states.
