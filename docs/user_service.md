# NebulaIM UserService

## Current Capability

UserService is the auth/profile service backed by MySQL and Redis. It supports Register, Login, ValidateToken, GetUserInfo, GetUserByUsername, Logout, and RefreshToken.

## Flow summary

Register:

```text
validate input -> check username -> PBKDF2 hash password -> UserDao create user -> return user_id
```

Login:

```text
load user by username -> verify password -> generate random token -> Redis SETEX -> optionally record device metadata -> return token
```

ValidateToken:

```text
token -> Redis GET nebula:token:{token} -> valid/user_id
```

GetUserInfo:

```text
user_id -> UserDao getUserById -> public UserInfo
```

GetUserByUsername:

```text
username -> UserDao getUserByUsername -> public UserInfo
```

Logout:

```text
token -> Redis DEL nebula:token:{token} -> return CommonResponse
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

Each password uses a unique random salt. Login returns generic `auth failed` for both missing user and wrong password. Logs never print plaintext passwords or full tokens.

## Config

```text
user_service.host=0.0.0.0
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

LoginRequest also accepts `device_id`, `platform`, and `device_name`; Gateway uses these fields for multi-device online state.

## Interview points

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
11. Logout deletes the token key.
12. Kickout deletes token and notifies gateway.
13. JWT is self-contained but harder to revoke; random token is centrally revocable.
14. UserDao centralizes SQL and escaping.
15. UserServiceContext centralizes dependency lifecycle.
