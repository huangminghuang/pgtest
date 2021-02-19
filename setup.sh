#!/bin/bash
export PGPORT=${PGPORT-5432}
export PGPASSWORD=${PGPASSWORD-password}
export PGUSER=postgres
export PGHOST=localhost

# psql -h localhost -U postgres
