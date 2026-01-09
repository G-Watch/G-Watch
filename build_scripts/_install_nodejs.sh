#!/bin/bash

set -e

if ! command -v "npm" >/dev/null 2>&1; then

# download and install nvm:
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash

# in lieu of restarting the shell
echo 'if [ -s "$HOME/.nvm/nvm.sh" ]; then
    . "$HOME/.nvm/nvm.sh"
fi' >> $HOME/.bashrc

. "$HOME/.nvm/nvm.sh"

# download and install Node.js:
nvm install 22

fi
