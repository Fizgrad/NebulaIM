# NebulaIM DeviceService

## Responsibilities

DeviceService is the internal device-management service. It reads device rows written during login, checks Redis online state, and revokes devices by clearing token and connection state. Closing the live Gateway connection is best effort; token and Redis cleanup determine whether revocation succeeds.

## Data Model

`user_devices` stores one row per `(user_id, device_id)`:

```text
user_id
device_id
platform
device_name
token_hash
last_login_at
last_active_at
created_at
updated_at
```

`token_hash` is `SHA-256(raw_token)`. The raw token is returned only to the client at login time and is never stored in MySQL.

## RPCs

- `ListDevices`: returns device metadata plus an `online` flag computed from Redis.
- `KickDevice`: revokes one device.
- `KickAllDevices`: revokes all known devices for a user.

DeviceService requires internal RPC metadata when `internal_rpc.auth.enabled=true`:

```text
x-nebula-internal-token: <raw-internal-token>
```

## Online State

DeviceService treats a device as online only when both keys exist:

```text
nebula:user:online:{user_id}:{device_id} -> gateway_id
nebula:user:conn:{user_id}:{device_id} -> connection_id
```

If either key is missing, DeviceService removes both per-device keys and prunes the device from:

```text
nebula:user:devices:{user_id}
```

## Revocation Flow

`KickDevice` and `KickAllDevices` perform the same steps for each target device:

```text
load user_devices row
resolve Redis online keys
if online: best-effort GatewayService.KickConnection(user_id, device_id, connection_id)
DEL nebula:token:{token_hash}
DEL online and connection keys
SREM nebula:user:devices:{user_id} device_id
clear user_devices.token_hash
```

GatewayService validates that the target connection belongs to the same `user_id` and `device_id` before closing it. If a stale Redis mapping points to an unavailable gateway, DeviceService still clears the token hash and online keys and reports success when the device row is updated.

## Run

```bash
./build/device_service/nebula_device_service --config config/nebula.conf
```

Default ports:

```text
device_service.port=50058
metrics.device_service.port=9107
```

`scripts/start_services.sh`, `scripts/wait_ready.sh`, `scripts/health_check.sh`, systemd units, and Prometheus config include DeviceService.
