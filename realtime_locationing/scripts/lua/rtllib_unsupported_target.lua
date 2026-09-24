-- Registered only when the project architecture matches none of the library:
-- entries in component/rtllib.slcc. Keep the two lists in sync.

validation.error(
  'Unsupported architecture',
  validation.target_for_project(),
  'The Real-Time Locationing library has no binary for this architecture. Supported: Cortex-M33, Linux x86-64, Linux aarch64, Linux armv7, and Windows x86-64.',
  nil
)
