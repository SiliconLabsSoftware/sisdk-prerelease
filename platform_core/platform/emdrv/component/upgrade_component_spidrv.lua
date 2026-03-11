local changeset = {}

if slc.is_provided('spidrv') then
  local replacement_already_present = slc.is_provided('spidrv_usart')
  
  -- If spidrv_usart is not present yet, add it with all instance names from spidrv
  if not replacement_already_present then
    local spidrv = slc.component('spidrv')
    local instance_names = {}

    -- Collect all instance names
    if (spidrv ~= nil) and (spidrv.instances ~= nil) then
      for instance_name, _ in pairs(spidrv.instances) do
        table.insert(instance_names, instance_name)
      end
    end

    -- Add spidrv_usart with all instance names
    if #instance_names > 0 then
      table.insert(changeset, {
        ['component'] = 'spidrv_usart',
        ['action'] = 'add',
        ['instance'] = instance_names,
      })
    end
  end

  -- Remove legacy spidrv
  table.insert(changeset, {
    ['component'] = 'spidrv',
    ['action'] = 'remove',
  })
end

return changeset
