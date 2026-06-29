# Deploy

Use `../scripts/start_deps.sh` for local development dependencies and `../scripts/start_services.sh` for local services.

For single-node production:

```bash
export NEBULA_MYSQL_ROOT_PASSWORD='replace-me'
export NEBULA_MYSQL_PASSWORD='replace-me'
export NEBULA_REDIS_PASSWORD='replace-me'
export NEBULA_GRAFANA_PASSWORD='replace-me'
../scripts/start_deps_prod.sh
sudo ../scripts/install_single_node.sh
```

See `../docs/single_node_production.md`.
