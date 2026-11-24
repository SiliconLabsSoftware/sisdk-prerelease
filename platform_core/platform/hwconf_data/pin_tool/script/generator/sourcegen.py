#!/usr/bin/env python3

import sys

# Utilities for Python 3
if sys.version_info[0] == 3:
  basestring = (str, bytes)

# Aliases for the signals
ALIASES = {
  'COPI': 'TX',
  'CIPO': 'RX',
}

def generate_requirement(region, req, module):
  # peripheral name / instance number
  if req.getType() in ['gpio'] and 'signal' not in req.getProperties():
    generate_gpio(region, module, req.getName())
  elif req.getType() in ['prs']:
    region.define('{}_CHANNEL'.format(req.getName()), get_module_instance(str(module.getName())))
    channel_type = get_prs_channel_type(module) or module.getProperty('channel.type')
    if channel_type:
      region.define('{}_CHANNEL_TYPE'.format(req.getName()), channel_type)
    region.write('')
    if 'gpio' in req.properties:
      signal = get_signals(module)[0]
      generate_signal(region, module, signal, req.getName())
  else:
    region.define('{}_PERIPHERAL'.format(req.getName()), module.getName())
    if get_module_instance(str(module.getName())):
        region.define('{}_PERIPHERAL_NO'.format(req.getName()), get_module_instance(str(module.getName())))

    # channel
    if 'channel' in req.properties:
      channels = req.properties['channel']
      if isinstance(channels, basestring):
        channels = [channels]
      else:
        channels = list(channels)

      for channel in channels:
        channel_count = module.getProperty('channel.count')
        for i in range(int(channel_count)):
          channel_name = module.getProperty('channel.{}.name'.format(i))
          if channel_name == channel:
            region.write('')
            region.define('{}_{}_CHANNEL'.format(req.getName(), channel), i)

            signal_names = module.getProperty('channel.signal').split(',')
            for j, signal_name in enumerate(signal_names):
              if j != 0:
                prefix = '_{}'.format(signal_name)
              else:
                prefix = ''
              generate_signal(region, module, '{}{}'.format(signal_name, i), '{}_{}{}'.format(req.getName(), channel, prefix))
            break # look for next required channel

    # signal
    if 'signal' in req.properties:
      signals = req.properties['signal']
      if isinstance(signals, basestring):
        signals = [signals]
      else:
        signals = list(signals)

      for signal in signals:
        if signal.startswith('('):
          # Strip parantheses indicating that the signal is optional
          signal = signal[1:-1]
        region.write('')
        generate_signal(region, module, signal, '{}_{}'.format(req.getName(), signal))


def get_signals(module):
  signals = []
  for selector in module.getMode().getPortIO().selectors:
    if selector.getModule() != module:
      # Not the desired module
      continue

    for route in selector.routes:
      signals.append(route.getName())

  return signals

def get_selectors(module):
  for selector in module.getMode().getPortIO().selectors:
    if selector.getModule() != module:
      # Not the desired module
      continue
    yield selector

def generate_signal(region, module, signal, label):
  have_output = False
  # Convert the signals if they have aliases
  if signal in ALIASES:
    signal = ALIASES[signal]
  for selector in module.getMode().getPortIO().selectors:
    if selector.getModule() != module:
      # Not the desired module
      continue

    # Construct location label
    if label.endswith(signal):
      location_label = label[:-len(signal)] + selector.getName().split('_')[-1]
    else:
      location_label = label

    if selector.locationPropertyReference:
      location = selector.locationPropertyReference.get()
    else:
      location = None

    for route in selector.routes:
      if route.getName() != signal:
        # Not the desired route
        continue

      if route.enablePropertyReference.enumIndex == 1:
        # Route is enabled
        for loc in route.locations:
          if (not location) or (str(loc.locationNumber) == location):
            # The selected location, or the only location if no location selector exists
            region.comment('{} {} on {}'.format(module.getName().split('.')[0], signal, loc.pin.getLabel()))
            region.define('{}_PORT'.format(label), loc.pin.getPortBank().getName())
            region.define('{}_PIN'.format(label), loc.pin.getIndex())
            if location is not None and loc.pin.getProperty('generate.location'):
              region.define('{}_LOC'.format(location_label), location)
            have_output = True

      # Finished processing matching route
      return have_output
  # Should never get here
  print('[WARNING] Signal {}/{} not available'.format(module.getName(), signal))
  # FIXME We do actually get here for CDTI on TIMERs that don't have one
  return have_output


def generate_gpio(region, pin, label):
  region.define('{}_PORT'.format(label), pin.getPortBank().getName())
  region.define('{}_PIN'.format(label), pin.getIndex())


def get_module_instance(name):
  instance = name[len(name.rstrip('0123456789')):]

  return instance

def _prs_short_name(name):
  return str(name).split('.')[-1].upper()


def get_prs_channel_type(module):
  """ASYNCH, SYNCH, or '' for legacy PRS.CHn names."""
  short = _prs_short_name(module.getName())
  if short.startswith('ASYNCH'):
    return 'ASYNCH'
  if short.startswith('SYNCH'):
    return 'SYNCH'
  return ''


def prs_channel_matches(module, channel, channel_type=None):
  instance = get_module_instance(str(module.getName()))
  try:
    if int(instance) != int(channel):
      return False
  except (TypeError, ValueError):
    if str(instance) != str(channel):
      return False

  kind = get_prs_channel_type(module)
  if channel_type:
    return kind == str(channel_type).upper()
  # Headers without CHANNEL_TYPE are async (or series-1 CH).
  return kind != 'SYNCH'