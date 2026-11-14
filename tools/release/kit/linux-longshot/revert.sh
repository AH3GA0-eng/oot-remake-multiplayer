#!/usr/bin/env bash
# Goes back to the version this kit had before its last update (data/docs/SETUP-LINUX.md, "Going back to an older
# version"): bash revert.sh. It runs scripts/update.sh --revert; bash scripts/update.sh --version v0.11.0 installs any
# older release instead.
echo 'revert.sh puts back the version this kit had before its last update.'
echo
echo '  Every player in a session needs the SAME version: the versions do not talk to each other.'
echo '  Tell your friends to go back too (their own revert.sh or Revert.bat), or you cannot play together.'
echo
echo '  Your saves, settings and coop-options.ini are kept, and so is this version:'
echo '  bash scripts/update.sh goes forward again. Starting the kit does not update back by itself.'
echo
exec bash "$(dirname "${BASH_SOURCE[0]}")/scripts/update.sh" --revert "$@"
