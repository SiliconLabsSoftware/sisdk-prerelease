#!/bin/sh
if [ $# == 0 ] ; then
  FILE=-
else
  FILE="$1"
fi
echo "pr2RadioCfg {00}"
cat $FILE \
  | egrep '#define RF_(GLOBAL|PREAMBLE|SYNC|MODEM|PA|SYNTH|FREQ|PKT|MATCH)' \
  | cut -d' ' -f3- \
  | sed -e 's:0x::g' -e 's:, ::g' -e 's:^:pr2RadioCfg {:' -e 's:$:}:'
echo "pr2RadioCfg {}"
