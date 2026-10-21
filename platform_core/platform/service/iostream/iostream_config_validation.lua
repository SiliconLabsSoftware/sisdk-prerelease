local iostream_vcom_present = slc.config("SL_IOSTREAM_USART_VCOM_CONFIG_H") ~= nil
iostream_vcom_present = iostream_vcom_present or slc.config("SL_IOSTREAM_UART_VCOM_CONFIG_H") ~= nil
iostream_vcom_present = iostream_vcom_present or slc.config("SL_IOSTREAM_EUSART_VCOM_CONFIG_H") ~= nil
iostream_vcom_present = iostream_vcom_present or slc.config("SL_IOSTREAM_LEUART_VCOM_CONFIG_H") ~= nil

if (slc.config("SL_BOARD_ENABLE_VCOM") == nil or slc.config("SL_BOARD_ENABLE_VCOM").value ~= "1") 
    and iostream_vcom_present then
        validation.error(
        "IOStream uses VCOM instance but VCOM is not enabled.",
        validation.target_for_defines({"SL_BOARD_ENABLE_VCOM"}),
        "Please enable VCOM in the Board Control component, or add \"--configuration SL_BOARD_ENABLE_VCOM:1\" to your project generation command.",
        nil)
  end