#ifndef GOPRO_PROTOCOL_H
#define GOPRO_PROTOCOL_H

// GoPro BLE Service and Characteristics UUIDs.
// The Control & Query service is advertised with 0xFEA6,
// which expands to "0000fea6-0000-1000-8000-00805f9b34fb".
#define GO_PRO_SERVICE_UUID      "0000fea6-0000-1000-8000-00805f9b34fb"

// Characteristic UUIDs for command exchange:
// GP_COMMAND_UUID is used for writing commands.
// GP_COMMAND_RESP_UUID is used for receiving notifications (responses).
#define GP_COMMAND_UUID          "b5f90072-aa8d-11e3-9046-0002a5d5c51b"
#define GP_COMMAND_RESP_UUID     "b5f90073-aa8d-11e3-9046-0002a5d5c51b"

#endif // GOPRO_PROTOCOL_H
