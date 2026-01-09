#!/bin/bash

set -e

if command -v "grafana-server" >/dev/null 2>&1; then
    echo "grafana already installed"
else
    # install prerequisite
    apt-get update -o APT::Sandbox::User=root
    apt-get install -y apt-transport-https software-properties-common wget -o APT::Sandbox::User=root
    mkdir -p /etc/apt/keyrings/
    wget -q -O - https://apt.grafana.com/gpg.key | gpg --dearmor | tee /etc/apt/keyrings/grafana.gpg > /dev/null
    echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main" | tee -a /etc/apt/sources.list.d/grafana.list
    echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com beta main" | tee -a /etc/apt/sources.list.d/grafana.list

    # updates the list of available packages
    apt-get update -o APT::Sandbox::User=root
    apt-get install -y grafana -o APT::Sandbox::User=root
fi
