import logging
from pathlib import Path

from ddp.rtt import SerialWire, SerialWireTimeoutError
import ddp.command
import ddp.response
from . import wisunpki
import wisun.command
import wisun.common
import wisun.response

# Default directory to search for DDP RAM binaries (ddp-host/demos/)
_DDP_RAM_SEARCH_DIR = Path(__file__).resolve().parent.parent / "demos"

ddp_session_list = {}

logger = logging.getLogger('executor')
logger.setLevel(logging.DEBUG)

def _find_ddp_app_for_board(board: str, search_dir: Path | None = None) -> str | None:
    directory = Path(search_dir) if search_dir is not None else _DDP_RAM_SEARCH_DIR
    candidate = directory / f"wisun_soc_ddp_ram-{board}.bin"
    if candidate.is_file():
        return str(candidate.resolve())
    return None

def get_ddp_ram_board_ids(search_dir: Path | None = None) -> list[str]:
    """Return a list of board IDs that have a DDP RAM binary in the search directory."""
    directory = Path(search_dir) if search_dir is not None else _DDP_RAM_SEARCH_DIR
    if not directory.is_dir():
        return []
    prefix = "wisun_soc_ddp_ram-"
    suffix = ".bin"
    board_ids = []
    for path in directory.iterdir():
        if path.is_file() and path.name.startswith(prefix) and path.name.endswith(suffix):
            board_id = path.name[len(prefix):-len(suffix)]
            board_ids.append(board_id)
    return sorted(board_ids)

def _get_jlink_session(jlink_serial = None, jlink_host = None, ddp_app_path = None, ddp_app_board = None):
    if ddp_app_path is None and ddp_app_board is not None:
        ddp_app_path = _find_ddp_app_for_board(ddp_app_board)
    params = { 'jlink_serial': jlink_serial, 'jlink_host': jlink_host }
    key = (jlink_serial, jlink_host)
    if key in ddp_session_list:
        # Return the existing SerialWire instance
        sw = ddp_session_list[key]
        sw.connect()
        try:
            sw.rtt_start()
        except SerialWireTimeoutError:
            sw.close()
            _remove_jlink_session(sw)
            raise
        return sw

    with open(ddp_app_path, 'rb') as f:
        ddp_app = f.read()

    # No session exists, start DDP application on the target. Device type is hardcoded
    # since all Series 2 devices work the same way
    sw = SerialWire('EFR32FG28AxxxF1024', **params)
    logger.info('Opening SerialWire connection to the device')
    try:
        sw.connect()
        logger.debug('Connection opened')
        sw.reset_and_halt()
        logger.info('Injecting provisioning application')
        sw.run_application(ddp_app)
        logger.debug('Provisioning application running')
        sw.rtt_start()
        # NVM3 is not initialized by default
        sw.nvm3_start_addr = None
        sw.nvm3_size = None
    except:
        sw.close()
        raise

    # Store session for re-use
    ddp_session_list[key] = sw
    return sw

def _remove_jlink_session(sw):
    """Remove the J-Link session from the session cache."""
    for key, cached_sw in list(ddp_session_list.items()):
        if cached_sw is sw:
            del ddp_session_list[key]
            break

def _initialize_nvm(sw, nvm3_start_addr = None, nvm3_size = None):
    if (sw.nvm3_start_addr == nvm3_start_addr) and (sw.nvm3_size == nvm3_size):
        logger.debug("NVM already initialized")
        return
    logger.info('Initializing NVM')
    tx = ddp.command.InitializeNvm(base_addr=nvm3_start_addr, nvm3_inst_size=nvm3_size)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    resp = ddp.response.InitializeNvm(rx)
    assert resp.status == 0, f'Failure during NVM initialization ({resp.status})'
    sw.nvm3_start_addr = nvm3_start_addr
    sw.nvm3_size = nvm3_size
    logger.debug(f'NVM initialized')

def get_pki():
    pki = wisunpki.get_pki()
    if not pki:
        return None
    if len(pki) == 3:
        return { "root": pki[0], "mca": pki[1], "mica": pki[2] }
    elif len(pki) == 2:
        return { "root": pki[0], "mica": pki[1] }
    elif len(pki) == 1:
        return { "root": pki[0] }
    else:
        return None

def create_pki(oem_company = None, oem_country = None, chain_length = None):
    wisunpki.create_pki(oem_name = oem_company, oem_country = oem_country, chain_length = chain_length)
    return get_pki()

def import_pki(root = None, mca = None, mica = None):
    pki = {}
    pki['root_cert'] = root['cert_path']
    if 'key_path' in root:
        pki['root_key'] = root['key_path']
    if mca:
        pki['mca_cert'] = mca['cert_path']
    if mica:
        pki['mica_cert'] = mica['cert_path']
        if 'key_path' in mica:
            pki['mica_key'] = mica['key_path']
    wisunpki.setup_pki(**pki)

def get_certificate(mac_address = None):
    return wisunpki.get_credential(hwserialnum = bytes.fromhex(mac_address))

def create_certificate_on_host(mac_address = None, oem_hwtype = None, device_type = None):
    if device_type == 'router':
        device_type_enum = wisunpki.WisunDeviceType.ROUTER
    else:
        device_type_enum = wisunpki.WisunDeviceType.BORDER_ROUTER
    return wisunpki.generate_on_host(hwtype_oid = oem_hwtype, hwserialnum=bytes.fromhex(mac_address), device_type = device_type_enum)

def create_certificate_on_device(mac_address = None, oem_hwtype = None, device_type = None, cert_id = None, key_id = None, nvm3_start_addr = None, nvm3_size = None, jlink_serial = None, jlink_host = None, ddp_app_path = None, ddp_app_board = None):
    if device_type == 'router':
        device_type_enum = wisunpki.WisunDeviceType.ROUTER
    else:
        device_type_enum = wisunpki.WisunDeviceType.BORDER_ROUTER
    sw = _get_jlink_session(jlink_serial = jlink_serial, jlink_host = jlink_host, ddp_app_path = ddp_app_path, ddp_app_board = ddp_app_board)
    try:
        # Initialize NVM3
        _initialize_nvm(sw, nvm3_start_addr = nvm3_start_addr, nvm3_size = nvm3_size)
        # Generate device key-pair
        logger.info('Generating device key pair on the device')
        tx = wisun.command.GenerateKeyPair(key_id)
        sw.rtt_send(tx)
        rx = sw.rtt_receive()
        resp = wisun.response.GenerateKeyPair(rx)
        assert resp.status in (0, 19), f'Failure during device key pair generation ({resp.status})'
        if resp.status == 19:
            logger.warning('Device key pair already exists')
        else:
            logger.debug('Device key pair generated')
        # Generate CSR
        logger.info('Generating CSR on the device')
        tx = wisun.command.GenerateCsr(key_id)
        sw.rtt_send(tx)
        rx = sw.rtt_receive()
        resp = wisun.response.GenerateCsr(rx)
        assert resp.status == 0, f'Failure during CSR generation ({resp.status})'
        logger.info('CSR generated')
        # Generate device certificate
        logger.info('Generating device certificate')
        device = wisunpki.generate_from_csr(hwtype_oid=oem_hwtype, hwserialnum=bytes.fromhex(mac_address), device_type = device_type_enum, csr_data=resp.csr)
        logger.info('Device certificate generated')
        # Write device certificate to NVM
        with open(device['cert_path'], 'rb') as f:
            cert_data = f.read()
            logger.info(f'Storing certificate to NVM Object ID 0x{cert_id:x}')
            tx = ddp.command.WriteNvm(cert_id, cert_data)
            sw.rtt_send(tx)
            rx = sw.rtt_receive()
            resp = ddp.response.WriteNvm(rx)
            assert resp.status == 0, f'Failure storing certificate to NVM ({resp.status})'
        logger.debug(f'Certificate stored')
    except SerialWireTimeoutError:
        _remove_jlink_session(sw)
        raise
    finally:
        sw.close()

def get_device(jlink_serial = None, jlink_host = None, ddp_app_path = None, ddp_app_board = None):
    sw = _get_jlink_session(jlink_serial = jlink_serial, jlink_host = jlink_host, ddp_app_path = ddp_app_path, ddp_app_board = ddp_app_board)
    try:
        logger.info('Retrieving device MAC address')
        mac_address = sw.get_mac_address().hex()
        logger.info('Retrieving device flash information')
        flash_size, flashpage_size = sw.get_flash_size()
        logger.info('Determining default NVM3 parameters')
        nvm3_start_addr, nvm3_size = wisun.common.get_default_nvm3_parameters(flash_size = flash_size, flashpage_size = flashpage_size)
    except SerialWireTimeoutError:
        _remove_jlink_session(sw)
        raise
    finally:
        sw.close()
    return { 'mac_address': mac_address, 'nvm3_start_addr': nvm3_start_addr, 'nvm3_size': nvm3_size }

def store_to_device(nvm3_size = None, nvm3_start_addr = None, certificates = [], keys = [], jlink_serial = None, jlink_host = None, ddp_app_path = None, ddp_app_board = None):
    sw = _get_jlink_session(jlink_serial = jlink_serial, jlink_host = jlink_host, ddp_app_path = ddp_app_path, ddp_app_board = ddp_app_board)
    try:
        # Initialize NVM3
        _initialize_nvm(sw, nvm3_start_addr = nvm3_start_addr, nvm3_size = nvm3_size)
        # Store certificates
        for cert in certificates:
            with open(cert['cert_path'], 'rb') as f:
                cert_data = f.read()
                logger.info(f'Storing certificate {cert["cert_path"]} to NVM Object ID 0x{cert["cert_id"]:x}')
                tx = ddp.command.WriteNvm(cert['cert_id'], cert_data)
                sw.rtt_send(tx)
                rx = sw.rtt_receive()
                resp = ddp.response.WriteNvm(rx)
                assert resp.status == 0, f'Failure storing certificate to NVM ({resp.status})'
                logger.debug(f'Certificate stored')
        # Store keys
        for key in keys:
            key_data = wisunpki.load_private_key_data(key_path = key["key_path"]).private_numbers().private_value.to_bytes(32, 'big')
            logger.info(f'Storing private key {key["key_path"]} to PSA Key ID 0x{key["key_id"]:x}')
            tx = wisun.command.InjectKey(key['key_id'], key_data)
            sw.rtt_send(tx)
            rx = sw.rtt_receive()
            resp = ddp.response.InjectKey(rx)
            assert resp.status == 0, f'Failure storing private key ({resp.status})'
            logger.info('Private key stored')
    except SerialWireTimeoutError:
        _remove_jlink_session(sw)
        raise
    finally:
        sw.close()
