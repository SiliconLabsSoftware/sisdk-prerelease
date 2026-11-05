The Python scripts and tests in this directory tree both generate and define the
components that make up the Pro2/Pro2+ chip configurator.


siradiocfg
==========

siradiocfg package contains hand-crafted modules that offer _one level_ higher
abstraction and encapsulation of siradioapi. The aim is to represent all radio
chips. siradiocfg modules use the siradioapi and si4440_modem_calculator package.

Additionally, siradiocfg modules allow direct use of siradioapi

siradioapi
=========

siradioapi package contains the generated Python modules that you can use to 
access and configure public properties and commands.

siradioapi_factory
=========

siradioapi_factory package contains the generated Python modules that you can
use to access and configure both public and internal properties and commands.

The Python code generation parses the pro2_object_api.xml (Object API Xml) to 
generate class definitions for each radio part defined in Object API Xml. Each
part-specific class contains instances of siradioapifoundation classes that 
implement the containment strategy and behavior of the per-part radio API
definition.

generate_object_api.bat
=======================

generate_object_api.bat aims to automate the generation of the Object API xml,
and as such is specific to the local workspace environment. Ideally, the 
Object API Xml would be an artifact of the radio firmware development, and 
retrieved by some dependency management magic. Until that time, it's necessary
to have a couple of Subversion repos checked out locally:

  * svn+ssh://svn.silabs.com/WES_Tools/library/pyapixml/trunk
     The xsd xslt files are sourced in src/xsd and src/xslt respectively.
     At some point in time these would need to be elsewhere as _tools_ specific
     
  * svn+ssh://svn.silabs.com/si4440_firmware/trunk/firmware
     This component contains the scripts that generate the master API xml from
     which Object API Xml is created via the xlst transform
     

siradio_api_py_generator.py
siradio_api_pygen_helper.py
pygen.py
===========================

These three modules implement the parsing of Object API Xml and the generation 
of modules in siradioapi. That's all I have to say about that.

TODOs:
=====
* Automate auto check-out and reconciliation of siradioapi/... when generating 
  code
 * Command#get_arg_stream_raw_bytes is not behaving correctly across variety
   of Fields.
 * Need to implement Command decoding of response stream.
 * Fully implement siradiocfg modules.
* etc.