# SoC - CBAP Dynamic Data Provisioning

This application enables provisioning the device with data, required by the Certificate Based Authentication and Pairing (CBAP) application. This is a RAM application, therefore it leaves the original firmware untouched.

## Prerequisites

1. Install the required python packages:

        pip install -r common/ddp/script/requirements.txt
2. Obtain root certificate. You can either use your own, or you can create it using this tool:

        python bluetooth_le_app/script/certificate_authorities/create_authority_certificate.py
  * This creates a root certificate with the default configuration under this directory:

        bluetooth_le_app/script/certificate_authorities/central_authority
  * About configuring the root certificate, or building certificate chain, please check the help message:

        python bluetooth_le_app/script/certificate_authorities/create_authority_certificate.py --help

## Usage

1. Build the project. Do not flash. Since this is a special application (RAM), it requires special handling.
2. Run the provisioning script:

        python provision.py --issuer_cert <root_certificate_path> --issuer_key <root_private_key_path>
  * To use the root certificate we created above:

        python provision.py --issuer_cert bluetooth_le_app/script/certificate_authorities/central_authority/certificate.pem --issuer_key bluetooth_le_app/script/certificate_authorities/central_authority/private_key.pem
  * For testing purposes, you can use the default (demo) root certificate. In this case, just omit these arguments:

        python provision.py

> Note: This root certificate is for demonstration purposes only, and it is not meant to be used in production.

  * To see how you can configure the device certificate and the RTT connection, please check the help message:

        python provision.py --help

This provisioning script
- establishes RTT connection to the device.
- uploads and runs the DDP RAM application.
- and finally, runs the DDP commands in order to:
  - Generate a device key pair.
  - Generate static authentication data.
  - Generate a common name, based on the device's UUID.
  - Build the device certificate and sign it with the issuer.
  - Inject the certificate into the device.
  - Inject the issuer (root) certificate into the device as well.

## Resources

[Certificate Based Authentication and Pairing (CBAP)](https://www.silabs.com/wireless/bluetooth/cbap)

[AN1396: Certificate-Based Bluetooth Authentication and Pairing](https://www.silabs.com/documents/public/application-notes/an1396-bluetooth-certificates.pdf)

[AN1268: Authenticating Silicon Labs Devices Using Device Certificates](https://www.silabs.com/documents/public/application-notes/an1268-efr32-secure-identity.pdf)

## Report Bugs & Get Support

You are always encouraged and welcome to report any issues you found to us via [Silicon Labs Community](https://www.silabs.com/community).