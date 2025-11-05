import bgapi
from os import path


class dfu_helper:
  connection = None

  def connect(self, com, bt_xapi=None, btmesh_xapi=None, *xapis, sdkpath=None):
    '''
    :param com: Name of the COM port
    :param bt_xapi: Path to the Bluetooth XAPI file.
    :param btmesh_xapi: Path to the Bluetooth mesh XAPI file.
    :param xapis: Further XAPI files.
    :param sdkpath: Path to the root of the SDK. It defaults to the relative path from the placement of this Python file
                    assuming that the Python interpreter is running in the script's directory.
                    In any case, the user can provide the value if they so please.
                    bt_xapi and btmesh_xapi must either be relative to the current working directory,
                    or absolute. They don't get concatenated with sdkpath if explicitly input.
    
    '''
    if sdkpath is None:
      sdkpath = path.normpath(path.join(path.dirname(__file__), '../../../..'))
    sdkpath = path.abspath(sdkpath)
    if bt_xapi is None:
      bt_xapi = path.join(sdkpath, 'protocol/bluetooth/api/sl_bt.xapi')
    if btmesh_xapi is None:
      btmesh_xapi = path.join(sdkpath, 'protocol/bluetooth/api/sl_btmesh.xapi')
    apis = [bt_xapi, btmesh_xapi]
    if xapis:
      apis.extend(xapis)
    # Normalize path
    apis = [path.normpath(api) for api in apis]
    self.connection = bgapi.BGLib(
        bgapi.SerialConnector(com), apis, event_handler=self.event_handler)
    self.connection.open()

  def event_handler(self, evt):
    print('Event reveived: {}'.format(evt))

  def disconnect(self):
    '''Closes the BGAPI connection'''
    self.connection.close()

  def set_connection(self, connection):
    '''If the user has an existing BGAPI connection, it can be used
    :param connection: Connection to use
    '''
    self.connection = connection

  def fwid_set(self, index, version_information, cid=0xFFFF):
    if index > 255:
      raise Exception("Index must be under 256!")
    if len(version_information) > 106:
      raise Exception("Version information is too long! (max 106)")
    rsp = self.connection.bt.user.message_to_target('F{idx:c}{len:c}{cid1:c}{cid0:c}{vinfo}'.format(
        idx=index, cid1=int(cid/256), cid0=(cid % 256), vinfo=version_information, len=2+len(version_information)))
    print(rsp)

  def uri_set(self, index, uri):
    if index > 255:
      raise Exception("Index must be under 256!")
    if len(uri) > 255:
      raise Exception("URI is too long! (max 255)")
    if len(uri) > 200:
      uri = [uri[:200], uri[200:]]
    else:
      uri = [uri]
    rsp = self.connection.bt.user.message_to_target(
        'U0{idx:c}{len:c}{uri}'.format(idx=index, len=len(uri[0]), uri=uri[0]))
    print(rsp)
    if len(uri) > 1:
      rsp = self.connection.bt.user.message_to_target(
          'U1{idx:c}{len:c}{uri}'.format(idx=index, len=len(uri[1]), uri=uri[1]))
      print(rsp)


def main(args=None, parser=None):
  '''
  Executes the default behavior of the module, i.e. extract CID, version information, and URI from
  arguments and set the values on the device via user commands.

  :param args: either arguments parsed with argparse.ArgumentParser, or a dictionary of values
  :param parser: argparse.ArgumentParser to add arguments to be parsed
  '''
  def auto_int(x):
    if isinstance(x, str):
      return int(x, 0)
    return int(x)

  def mygetattr(x, name, _default=None):
    ''' Wraps getattr or dictionary access, so they can be treated in the same way. '''
    if isinstance(x, dict):
      return x[name] if name in x.keys() else _default
    return getattr(x, name, _default)
  
  if parser is None:
    import argparse
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('-c', '--com', help='Serial port to connect to', required=True)
  parser.add_argument('-i', '--index', help='Index of the firmware', default=0, type=auto_int)
  parser.add_argument('-u', '--uri', help='Firmware URI')
  parser.add_argument('-v', '--version_info', help='Version information part of the firmware identifier')
  parser.add_argument('-cid', '--company_id', help='Company ID (as a number) part of the firmware identifier. (default: 0xFFFF)', type=auto_int)
  parser.add_argument('-sdk', '--sdk_path', help='Path to the SDK. If missing, a relative path will be used from this script.')
  parser.add_argument('-bt', '--bt_xapi', help='Path to sl_bt.xapi file. If missing, an SDK relative path will be used.')
  parser.add_argument('-bm', '--bt_mesh_xapi', help='Path to sl_btmesh.xapi file. If missing, an SDK relative path will be used.')
  parser.add_argument('-x', '--xapi', nargs='*', help='Further XAPI files')
  parser.add_argument('-j', '--json', nargs='*', help='''JSON file(s) containing data in the following format:
  [
    {
      "ver_info": "This is the version information",
      "uri": "https://my_website.com/dfu?fw-uri"
      "cid": "0x1234"
    },
    {
      "ver_info": "This is the another version information",
      "uri": "https://my_website.com/dfu?another-fw-uri"
      "cid": 4660
    }
  ]
  The fields are optional.
  CID can be a decimal integer or a string representing a string literal. (4660 = 0x1234)
  The index is positional, if more files are given, their position determines the indexing.
  If the -i/--index argument is present, it'll be used as a starting index.
  If either -u/--uri, -cid/--company_id, or -v/--version_info is present, it'll take either index 0 or the given index.''')

  if args is None:
    args = parser.parse_args()

  index = mygetattr(args,"index")
  val = {}
  if mygetattr(args,"company_id") is not None:
    val["cid"] = mygetattr(args,"company_id")
  if mygetattr(args,"version_info") is not None:
    val["ver_info"] = mygetattr(args,"version_info")
  if mygetattr(args,"uri") is not None:
    val["uri"] = mygetattr(args,"uri")

  if val:
    values = [val]
  else:
    values = []

  # Parse JSON
  if mygetattr(args,"json") is not None:
    import json
    for val in mygetattr(args,"json"):
      with open(val, 'r') as f:
        val = f.read()
      val = json.loads(val)
      values.extend(val)

  # Sanitize
  for val in values:
    if "cid" in val:
      val["cid"] = auto_int(val["cid"])
    else:
      val["cid"] = 0xFFFF
    if "ver_info" not in val:
      val["ver_info"] = ''
    if "uri" not in val:
      val["uri"] = ''

  x = dfu_helper()
  x.connect(mygetattr(args,"com"), mygetattr(args,"bt_xapi"), mygetattr(args,"bt_mesh_xapi"), sdkpath=mygetattr(args,"sdk_path"))

  for i in range(len(values)):
    x.fwid_set(index + i, values[i]["ver_info"], values[i]["cid"])
    if values[i]["uri"]:
      x.uri_set(index + i, values[i]["uri"])


if __name__ == "__main__":
  main()