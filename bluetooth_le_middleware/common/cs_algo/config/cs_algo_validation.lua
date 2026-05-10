-- cs_algo validation script for checking algo configuration validity.
-- Each Bluetooth LE connection may own 0 or 1 CS algo estimators, therefore
-- CS_ALGO_ESTIMATOR_COUNT must:
--   * be in the range [1 .. SL_BT_CONFIG_MAX_CONNECTIONS], and
--   * not exceed CS_INITIATOR_MAX_CONNECTIONS, since estimators are only
--     created for initiator connections (the number of created estimators
--     cannot be larger than the number of initiator connections).
local modify_msg = "Modify cs_algo_config.h!"

local cs_algo_estimator_cnt =
  slc.config('CS_ALGO_ESTIMATOR_COUNT').number
local bt_max_conn =
  slc.config('SL_BT_CONFIG_MAX_CONNECTIONS').number
local cs_initiator_max_conn =
  slc.config('CS_INITIATOR_MAX_CONNECTIONS').number

-- Range check vs. max connection count: there should be at least one estimator,
-- number of estimators cannot be larger than the number of connections
if cs_algo_estimator_cnt ~= nil and bt_max_conn ~= nil then
  if not (cs_algo_estimator_cnt >= 1 and cs_algo_estimator_cnt <= bt_max_conn) then
    validation.error(
    "Invalid CS algo estimator count!",
    validation.target_for_defines({'CS_ALGO_ESTIMATOR_COUNT'}),
    [[CS_ALGO_ESTIMATOR_COUNT (]] .. cs_algo_estimator_cnt .. [[) is out of range!
    Valid range is 1 to SL_BT_CONFIG_MAX_CONNECTIONS (]] .. bt_max_conn .. [[). ]] .. modify_msg,
    nil)
  end
end

-- The number of created estimators cannot exceed the
-- number of initiator connections.
if cs_algo_estimator_cnt ~= nil and cs_initiator_max_conn ~= nil then
  if cs_algo_estimator_cnt > cs_initiator_max_conn then
    validation.error(
    "CS algo estimator count exceeds initiator connection count!",
    validation.target_for_defines({'CS_ALGO_ESTIMATOR_COUNT', 'CS_INITIATOR_MAX_CONNECTIONS'}),
    [[CS_ALGO_ESTIMATOR_COUNT (]] .. cs_algo_estimator_cnt .. [[) must not exceed
    CS_INITIATOR_MAX_CONNECTIONS (]] .. cs_initiator_max_conn .. [[).
    Estimators are only created for initiator connections, so the number of created
    estimators cannot be larger than the number of initiator connections. ]] .. modify_msg,
    nil)
  end
end
