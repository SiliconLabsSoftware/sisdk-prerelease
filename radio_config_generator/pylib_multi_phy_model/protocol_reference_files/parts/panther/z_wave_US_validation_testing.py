
from pylib_multi_phy_model.protocol_reference_files.utils.iProtocol import *

class z_wave_US_validation_testing(IProtocol):

    def get_model(self):
        mphy_config = multi_phy_configuration(part_family="panther",
                                                 part_revision="A0",
                                                 desc="z_wave_US_validation_testing",
                                                 rail_adapter_version="rail_api_2.x",
                                                 xsd_version=MultiPHYConfig.xsd_version(),
                                                 status_code=ModelDiffCodes.OK.value,
                                                 readable_name="Z-wave US validation testing")

        base_channel_configuration = base_channel_configurationType(name="Z-Wave US validation testing with 3 Channels", profile=None)
        phy_name = ""
        base_channel_configuration.phy = phyType(phy_name)

        profile_input_overrides = base_channel_configuration.phy.profile_input_overrides

        channel_config_entry = channel_config_entryType(name="PHY_ZWave_100kbps_916MHz_US_ch0",
                                            phy_name_override="PHY_ZWave_100kbps_916MHz",
                                            base_frequency=916000000,
                                            channel_spacing=0,
                                            physical_channel_offset=5,
                                            channel_number_start=0,
                                            channel_number_end=1,
                                            max_power=-2)


        base_channel_configuration.channel_config_entries.add_channel_config_entry(channel_config_entry)

        channel_config_entry = channel_config_entryType(name="PHY_ZWave_40kbps_908MHz_US_ch1",
                                            phy_name_override="PHY_ZWave_40kbps_908MHz",
                                            base_frequency=908400000,
                                            channel_spacing=0,
                                            physical_channel_offset=3,
                                            channel_number_start=1,
                                            channel_number_end=2,
                                            max_power=-1)
        base_channel_configuration.channel_config_entries.add_channel_config_entry(channel_config_entry)

        channel_config_entry = channel_config_entryType(name="PHY_ZWave_9p6kbps_908MHz_US_ch2",
                                            phy_name_override="PHY_ZWave_9p6kbps_908MHz",
                                            base_frequency=908420000,
                                            channel_spacing=0,
                                            physical_channel_offset=0,
                                            channel_number_start=2,
                                            channel_number_end=3,
                                            max_power=0)
        base_channel_configuration.channel_config_entries.add_channel_config_entry(channel_config_entry)

        mphy_config.base_channel_configurations.add_base_channel_configuration(base_channel_configuration)

        return mphy_config

