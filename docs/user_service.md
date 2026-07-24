# NebulaIM UserService

## Current Capability

UserService is the auth/profile service backed by MySQL and Redis. It supports Register, Login, ValidateToken, GetUserInfo, GetUserByUsername, RefreshToken, and login-time device metadata persistence. DeviceService owns logout and device revocation.

## Flow summary

Register:

```text
validate input -> check username -> PBKDF2 hash password -> UserDao create user -> return user_id
```

Login:

```text
load user by username -> verify password -> generate random token -> Redis SETEX -> record device metadata when device_id is present -> return token
```

ValidateToken:

```text
token -> SHA-256(token) -> Redis GET nebula:token:{token_hash} -> valid/user_id
```

GetUserInfo:

```text
user_id -> UserDao getUserById -> public UserInfo
```

GetUserByUsername:

```text
username -> UserDao getUserByUsername -> public UserInfo
```

RefreshToken:

```text
old token -> ValidateToken -> generate new random token -> SETEX new token -> DEL old token
```

## Security notes

Passwords are stored as:

```text
pbkdf2_sha256$iterations$salt_hex$hash_hex
```

Each password uses a unique random salt. Login returns generic `auth failed` for both missing user and wrong password. Logs never print plaintext passwords or full tokens. Redis token keys use `SHA-256(token)` rather than the raw bearer token, so a Redis key scan does not directly expose live tokens.

## Config

```text
user_service.host=127.0.0.1
user_service.port=50051
auth.token_ttl_seconds=604800
auth.password_min_length=6
```

## Run

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
```

```bash
./build/user_service/nebula_user_service --config config/nebula.conf
./build/examples/user_register_login_client --addr 127.0.0.1:50051
```

## Tests

```bash
./build/tests/test_password_hasher
./build/tests/test_token_manager
./build/tests/test_user_service_impl
./build/tests/test_user_service_integration
```

`test_user_service_impl` and `test_user_service_integration` require MySQL and Redis.

LoginRequest also accepts `device_id`, `platform`, and `device_name`. When `device_id` is present, UserService writes `user_devices.token_hash=SHA-256(token)` so DeviceService can revoke the token without storing the raw token.

## Knowledge checks

1. Username uniqueness relies on both pre-check and DB unique index.
2. Passwords must not be stored in plaintext.
3. Salt makes identical passwords produce different hashes.
4. PBKDF2 is portable; bcrypt/scrypt/Argon2 are stronger production choices.
5. Login should not reveal whether the username exists.
6. Token TTL limits token leakage risk.
7. Redis is suitable for token storage because it supports low-latency TTL lookup.
8. Redis failure during login should fail the login because token persistence failed.
9. ValidateToken can be optimized with short-lived local cache if Redis is hot.
10. Multi-device login can store multiple random tokens per user/device.
11. DeviceService revocation deletes the hashed token key and online state.
12. DeviceService also requests Gateway connection closure and reports an unconfirmed live closure.
13. JWT is self-contained but harder to revoke; random token is centrally revocable.
14. UserDao centralizes SQL and escaping.
15. UserServiceContext centralizes dependency lifecycle.
